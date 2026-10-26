#pragma once

#include <d3d11_1.h>
#include <wrl/client.h>

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

namespace haylen::gpu::d3d11 {

// A dynamic buffer that takes data for the GPU one piece after another, mapping each piece without waiting: the pieces append behind the ones the GPU may still read, and when the ring is full it starts over on a fresh copy of the buffer that the driver hands out, while the GPU keeps reading the old one. Uniforms bind parts of it as constant buffers.
class DynamicRing final {
  public:
    DynamicRing(ID3D11Device& owner, UINT size, UINT bindFlags, UINT alignment, std::string_view label);

    // Copies the data into the ring and returns its offset in bytes, the room it takes rounded up to the alignment, or nothing when the device was lost. A piece larger than the ring makes it grow first.
    [[nodiscard]] std::optional<UINT> write(ID3D11DeviceContext& context, std::span<const std::byte> data);

    [[nodiscard]] ID3D11Buffer* get() const noexcept {
        return buffer.Get();
    }

    [[nodiscard]] UINT getAlignment() const noexcept {
        return alignment;
    }

  private:
    void make(UINT bytes);

    ID3D11Device& device;
    Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
    std::string_view name;
    UINT size = 0;
    UINT bind = 0;
    UINT alignment = 0;
    UINT cursor = 0;
};

} // namespace haylen::gpu::d3d11
