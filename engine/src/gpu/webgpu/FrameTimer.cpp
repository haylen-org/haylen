#include "gpu/webgpu/FrameTimer.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <span>

#include "gpu/webgpu/Conversion.hpp"

namespace haylen::gpu::webgpu {

FrameTimer::FrameTimer(WGPUDevice device) {
    WGPUQuerySetDescriptor queryDescriptor = WGPU_QUERY_SET_DESCRIPTOR_INIT;
    queryDescriptor.label = Conversion::toText("haylen-frame-timer");
    queryDescriptor.type = WGPUQueryType_Timestamp;
    queryDescriptor.count = kSlots * kMaxPasses * 2U;
    queries = wgpuDeviceCreateQuerySet(device, &queryDescriptor);

    WGPUBufferDescriptor resolvedDescriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
    resolvedDescriptor.label = Conversion::toText("haylen-frame-timer-resolve");
    resolvedDescriptor.usage = WGPUBufferUsage_QueryResolve | WGPUBufferUsage_CopySrc;
    resolvedDescriptor.size = kSlots * kSlotBytes;
    resolved = wgpuDeviceCreateBuffer(device, &resolvedDescriptor);

    WGPUBufferDescriptor readbackDescriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
    readbackDescriptor.label = Conversion::toText("haylen-frame-timer-readback");
    readbackDescriptor.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    readbackDescriptor.size = kSlotBytes;
    for (Slot& slot : slots) {
        slot.readback = wgpuDeviceCreateBuffer(device, &readbackDescriptor);
    }
    writes.querySet = queries;
}

FrameTimer::~FrameTimer() {
    for (const Slot& slot : slots) {
        wgpuBufferDestroy(slot.readback);
        wgpuBufferRelease(slot.readback);
    }
    wgpuBufferDestroy(resolved);
    wgpuBufferRelease(resolved);
    wgpuQuerySetDestroy(queries);
    wgpuQuerySetRelease(queries);
}

void FrameTimer::receiveMapping(WGPUMapAsyncStatus status, WGPUStringView, void* first, void* second) {
    auto& timer = *static_cast<FrameTimer*>(first);
    timer.states[reinterpret_cast<std::uintptr_t>(second)] = status == WGPUMapAsyncStatus_Success ? State::Mapped : State::Free;
}

// The frame takes its slot only while the slot is free, so the queries of a slot that still waits for its mapping are never written again.
const WGPUPassTimestampWrites* FrameTimer::nextPass() noexcept {
    if (!started) {
        started = true;
        collect();
        measuring = states[current] == State::Free;
    }
    Slot& slot = slots[current];
    if (!measuring || slot.passes == kMaxPasses) {
        return nullptr;
    }
    const std::uint32_t first = (current * kMaxPasses + slot.passes) * 2U;
    writes.beginningOfPassWriteIndex = first;
    writes.endOfPassWriteIndex = first + 1U;
    ++slot.passes;
    return &writes;
}

void FrameTimer::resolve(WGPUCommandEncoder encoder) noexcept {
    const Slot& slot = slots[current];
    if (slot.passes == 0) {
        return;
    }
    const std::uint64_t offset = current * kSlotBytes;
    wgpuCommandEncoderResolveQuerySet(encoder, queries, current * kMaxPasses * 2U, slot.passes * 2U, resolved, offset);
    wgpuCommandEncoderCopyBufferToBuffer(encoder, resolved, offset, slot.readback, 0, std::uint64_t{slot.passes} * 2U * sizeof(std::uint64_t));
}

void FrameTimer::submitted() noexcept {
    Slot& slot = slots[current];
    ++submissions;
    if (slot.passes != 0) {
        slot.mapped = slot.passes;
        slot.frame = submissions;
        states[current] = State::Mapping;
        WGPUBufferMapCallbackInfo callback = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
        callback.mode = WGPUCallbackMode_AllowSpontaneous;
        callback.callback = &FrameTimer::receiveMapping;
        callback.userdata1 = this;
        callback.userdata2 = reinterpret_cast<void*>(static_cast<std::uintptr_t>(current));
        wgpuBufferMapAsync(slot.readback, WGPUMapMode_Read, 0, static_cast<std::size_t>(kSlotBytes), callback);
    }
    current = (current + 1U) % kSlots;
    slots[current].passes = 0;
    started = false;
    measuring = false;
}

// The slots finish their mappings in any order, while the timeline takes the frames in the order they were submitted.
void FrameTimer::collect() noexcept {
    std::array<Slot*, kSlots> mapped{};
    std::size_t count = 0;
    for (std::uint32_t index = 0; index < kSlots; ++index) {
        if (states[index] == State::Mapped) {
            mapped[count++] = &slots[index];
            states[index] = State::Free;
        }
    }
    std::sort(mapped.begin(), mapped.begin() + static_cast<std::ptrdiff_t>(count), [](const Slot* first, const Slot* second) { return first->frame < second->frame; });
    for (std::size_t index = 0; index < count; ++index) {
        read(*mapped[index]);
    }
}

// A frame whose timestamps cannot tell its time leaves the time of the last frame that could.
void FrameTimer::read(Slot& slot) noexcept {
    std::array<std::uint64_t, kMaxPasses * 2U> times{};
    std::memcpy(times.data(), wgpuBufferGetConstMappedRange(slot.readback, 0, static_cast<std::size_t>(kSlotBytes)), sizeof(times));
    wgpuBufferUnmap(slot.readback);
    if (const std::optional<double> frame = timeline.addFrame(slot.frame, std::span(times).first(std::size_t{slot.mapped} * 2U))) {
        seconds = frame;
    }
}

std::optional<double> FrameTimer::getSeconds() const noexcept {
    return seconds;
}

} // namespace haylen::gpu::webgpu
