#include "gpu/webgpu/FrameTimer.hpp"

#include <cstddef>
#include <cstring>

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
    if (slot.passes != 0) {
        slot.mapped = slot.passes;
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

// The time of a frame adds up the times of its passes, which leaves out the gaps between them, when the GPU ran other work.
void FrameTimer::collect() noexcept {
    for (std::uint32_t index = 0; index < kSlots; ++index) {
        if (states[index] != State::Mapped) {
            continue;
        }
        Slot& slot = slots[index];
        std::array<std::uint64_t, kMaxPasses * 2U> times{};
        std::memcpy(times.data(), wgpuBufferGetConstMappedRange(slot.readback, 0, static_cast<std::size_t>(kSlotBytes)), sizeof(times));
        wgpuBufferUnmap(slot.readback);
        states[index] = State::Free;

        std::uint64_t total = 0;
        for (std::size_t pass = 0; pass < slot.mapped; ++pass) {
            const std::uint64_t begin = times[pass * 2U];
            const std::uint64_t end = times[pass * 2U + 1U];
            total += end > begin ? end - begin : 0U;
        }
        seconds = static_cast<double>(total) * 1e-9;
    }
}

std::optional<double> FrameTimer::getSeconds() const noexcept {
    return seconds;
}

} // namespace haylen::gpu::webgpu
