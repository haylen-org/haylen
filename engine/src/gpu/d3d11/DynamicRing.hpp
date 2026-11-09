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
    // Where a piece went in the ring, and whether the ring started over on a fresh copy for it, which holds none of the pieces written before.
    struct Piece {
        UINT offset = 0;
        bool restarted = false;
    };

    DynamicRing(ID3D11Device& owner, UINT size, UINT bindFlags, UINT alignment, std::string_view label);

    // Copies the data, which the ring always has room for, and returns where it went, or nothing when the GPU stopped working. Throws `std::runtime_error` when the driver refuses to map the buffer for another reason.
    [[nodiscard]] std::optional<Piece> write(ID3D11DeviceContext& context, std::span<const std::byte> data);

    [[nodiscard]] ID3D11Buffer* get() const noexcept {
        return buffer.Get();
    }

  private:
    Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
    UINT size = 0;
    UINT alignment = 0;
    UINT cursor = 0;
};

} // namespace haylen::gpu::d3d11
