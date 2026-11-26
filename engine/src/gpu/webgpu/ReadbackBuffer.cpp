#include "gpu/webgpu/ReadbackBuffer.hpp"

#include <stdexcept>
#include <string>

#include "gpu/webgpu/Conversion.hpp"

namespace haylen::gpu::webgpu {

ReadbackBuffer::ReadbackBuffer(WGPUDevice device, std::uint64_t bytes, std::string_view label) : size(bytes) {
    WGPUBufferDescriptor descriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
    descriptor.label = Conversion::toText(label);
    descriptor.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    descriptor.size = size;
    buffer = wgpuDeviceCreateBuffer(device, &descriptor);
}

ReadbackBuffer::~ReadbackBuffer() {
    wgpuBufferDestroy(buffer);
    wgpuBufferRelease(buffer);
}

WGPUBuffer ReadbackBuffer::getBuffer() const noexcept {
    return buffer;
}

void ReadbackBuffer::submit(WGPUQueue queue, WGPUCommandEncoder encoder) {
    WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuQueueSubmit(queue, 1, &commands);
    wgpuCommandBufferRelease(commands);
    wgpuCommandEncoderRelease(encoder);

    WGPUBufferMapCallbackInfo callback = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
    callback.mode = WGPUCallbackMode_AllowSpontaneous;
    callback.callback = &ReadbackBuffer::receiveMapping;
    callback.userdata1 = &state;
    wgpuBufferMapAsync(buffer, WGPUMapMode_Read, 0, static_cast<std::size_t>(size), callback);
}

void ReadbackBuffer::receiveMapping(WGPUMapAsyncStatus status, WGPUStringView, void* first, void*) {
    *static_cast<State*>(first) = status == WGPUMapAsyncStatus_Success ? State::Mapped : State::Failed;
}

std::optional<std::span<const std::byte>> ReadbackBuffer::getMapped(std::string_view failure) const {
    switch (state) {
    case State::Mapping:
    case State::Delivered:
        return std::nullopt;
    case State::Failed:
        throw std::runtime_error(std::string(failure));
    case State::Mapped:
        break;
    }

    const auto* mapped = static_cast<const std::byte*>(wgpuBufferGetConstMappedRange(buffer, 0, static_cast<std::size_t>(size)));
    return std::span<const std::byte>(mapped, static_cast<std::size_t>(size));
}

void ReadbackBuffer::deliver() noexcept {
    wgpuBufferUnmap(buffer);
    state = State::Delivered;
}

} // namespace haylen::gpu::webgpu
