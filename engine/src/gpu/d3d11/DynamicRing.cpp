#include "gpu/d3d11/DynamicRing.hpp"

#include <cstring>

#include "gpu/direct3d/HResult.hpp"

namespace haylen::gpu::d3d11 {

// The first piece goes in with a discard, since the ring starts full.
DynamicRing::DynamicRing(ID3D11Device& owner, UINT bytes, UINT bindFlags, UINT pieceAlignment, std::string_view label) : size(bytes), alignment(pieceAlignment), cursor(bytes) {
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = bytes;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = bindFlags;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    direct3d::HResult::check(owner.CreateBuffer(&desc, nullptr, buffer.GetAddressOf()), "create a ring of dynamic data");
    buffer->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(label.size()), label.data());
}

std::optional<DynamicRing::Piece> DynamicRing::write(ID3D11DeviceContext& context, std::span<const std::byte> data) {
    const auto room = static_cast<UINT>((data.size() + alignment - 1) / alignment * alignment);

    // The first piece after a wrap discards the buffer, and the others leave what the GPU reads untouched.
    Piece piece{.offset = cursor};
    D3D11_MAP mode = D3D11_MAP_WRITE_NO_OVERWRITE;
    if (cursor + room > size) {
        mode = D3D11_MAP_WRITE_DISCARD;
        piece = {.offset = 0, .restarted = true};
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    const HRESULT result = context.Map(buffer.Get(), 0, mode, 0, &mapped);
    if (direct3d::HResult::isDeviceLoss(result)) {
        return std::nullopt;
    }
    direct3d::HResult::check(result, "map the ring of dynamic data");
    std::memcpy(static_cast<std::byte*>(mapped.pData) + piece.offset, data.data(), data.size());
    context.Unmap(buffer.Get(), 0);

    cursor = piece.offset + room;
    return piece;
}

} // namespace haylen::gpu::d3d11
