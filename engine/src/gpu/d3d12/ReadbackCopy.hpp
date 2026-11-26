#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace haylen::gpu::d3d12 {

// A copy into a buffer of a readback heap, which maps the buffer only once the fence passed the frame that copied it, so it never waits. The readbacks of render targets and buffers read through it.
class ReadbackCopy final {
  public:
    ReadbackCopy(Microsoft::WRL::ComPtr<ID3D12Resource> copy, Microsoft::WRL::ComPtr<ID3D12Fence> frames, std::uint64_t frameValue) noexcept;

    // Maps the first bytes of the buffer for reading once the GPU finished the copy, or returns nothing while it runs. A removed device reports every fence value as passed, and its buffers then refuse to map, which throws `std::runtime_error` with the failure.
    [[nodiscard]] std::optional<std::span<const std::byte>> map(std::size_t bytes, std::string_view failure);
    void unmap() noexcept;

  private:
    Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    std::uint64_t value = 0;
};

} // namespace haylen::gpu::d3d12
