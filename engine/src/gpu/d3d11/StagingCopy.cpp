#include "gpu/d3d11/StagingCopy.hpp"

#include <stdexcept>
#include <string>
#include <utility>

#include "gpu/direct3d/HResult.hpp"

namespace haylen::gpu::d3d11 {

StagingCopy::StagingCopy(ID3D11Device& device, Microsoft::WRL::ComPtr<ID3D11DeviceContext> owner, Microsoft::WRL::ComPtr<ID3D11Resource> copy) : context(std::move(owner)), staging(std::move(copy)) {
    const D3D11_QUERY_DESC event{.Query = D3D11_QUERY_EVENT, .MiscFlags = 0};
    direct3d::HResult::check(device.CreateQuery(&event, done.GetAddressOf()), "create the query of a readback");
    context->End(done.Get());
}

std::optional<D3D11_MAPPED_SUBRESOURCE> StagingCopy::map(std::string_view failure) {
    if (context->GetData(done.Get(), nullptr, 0, D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_FALSE) {
        return std::nullopt;
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    const HRESULT result = context->Map(staging.Get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped);
    if (result == DXGI_ERROR_WAS_STILL_DRAWING) {
        return std::nullopt;
    }

    if (FAILED(result)) {
        throw std::runtime_error(std::string(failure));
    }

    return mapped;
}

void StagingCopy::unmap() noexcept {
    context->Unmap(staging.Get(), 0);
}

} // namespace haylen::gpu::d3d11
