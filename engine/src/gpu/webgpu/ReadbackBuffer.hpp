#pragma once

#include <webgpu/webgpu.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace haylen::gpu::webgpu {

// A buffer that the CPU maps once the GPU finished the copy into it, without waiting for it, which the readbacks of images and buffers copy into. A buffer that is destroyed ends its mapping before the call returns, so the callback of the mapping never outlives the buffer, and a lost device ends the mapping as failed.
class ReadbackBuffer final {
  public:
    ReadbackBuffer(WGPUDevice device, std::uint64_t bytes, std::string_view label);
    ReadbackBuffer(const ReadbackBuffer&) = delete;
    ReadbackBuffer& operator=(const ReadbackBuffer&) = delete;
    ~ReadbackBuffer();

    [[nodiscard]] WGPUBuffer getBuffer() const noexcept;

    // Submits the commands of the encoder that copy into the buffer on their own and starts mapping the buffer, which is ready once the GPU finished them.
    void submit(WGPUQueue queue, WGPUCommandEncoder encoder);

    // The bytes of the buffer from the moment it is mapped until `deliver`, and none while the GPU works on the copy or after the bytes were handed over. Throws `std::runtime_error` with the sentence of the failure when the mapping failed.
    [[nodiscard]] std::optional<std::span<const std::byte>> getMapped(std::string_view failure) const;

    // Unmaps the buffer once its bytes were handed over.
    void deliver() noexcept;

  private:
    // Where the mapping stands, which its callback sets, and whether the bytes were handed over.
    enum class State : std::uint8_t {
        Mapping,
        Mapped,
        Failed,
        Delivered,
    };

    static void receiveMapping(WGPUMapAsyncStatus status, WGPUStringView message, void* first, void* second);

    WGPUBuffer buffer = nullptr;
    std::uint64_t size;
    State state = State::Mapping;
};

} // namespace haylen::gpu::webgpu
