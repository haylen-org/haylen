#include "gpu/d3d11/DynamicRing.hpp"

#include <bit>
#include <cstring>

#include "gpu/direct3d/HResult.hpp"

namespace haylen::gpu::d3d11 {

DynamicRing::DynamicRing(ID3D11Device& owner, UINT bytes, UINT bindFlags, UINT pieceAlignment, std::string_view label) : device(owner), name(label), bind(bindFlags), alignment(pieceAlignment) {
    make(bytes);
}

void DynamicRing::make(UINT bytes) {
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = bytes;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = bind;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    buffer.Reset();
    direct3d::HResult::check(device.CreateBuffer(&desc, nullptr, buffer.GetAddressOf()), "create a ring of dynamic data");
    buffer->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(name.size()), name.data());
    size = bytes;
    cursor = size;
}

std::optional<UINT> DynamicRing::write(ID3D11DeviceContext& context, std::span<const std::byte> data) {
    const auto room = static_cast<UINT>((data.size() + alignment - 1) / alignment * alignment);
    if (room > size) {
        make(std::bit_ceil(room));
    }

    // The first piece after a start or a wrap discards the buffer, and the others leave what the GPU reads untouched.
    D3D11_MAP mode = D3D11_MAP_WRITE_NO_OVERWRITE;
    if (cursor + room > size) {
        mode = D3D11_MAP_WRITE_DISCARD;
        cursor = 0;
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context.Map(buffer.Get(), 0, mode, 0, &mapped))) {
        return std::nullopt;
    }
    std::memcpy(static_cast<std::byte*>(mapped.pData) + cursor, data.data(), data.size());
    context.Unmap(buffer.Get(), 0);

    const UINT offset = cursor;
    cursor += room;
    return offset;
}

} // namespace haylen::gpu::d3d11
