#pragma once

#include <d3d11_1.h>
#include <wrl/client.h>

#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::d3d11 {

// Makes the state objects of Direct3D 11 that pipelines and samplers describe. The runtime hands out one object for every equal description, so pipelines that share a blend, rasterizer or depth state share its object, and the context skips setting an object it holds already.
class StateObjects final {
  public:
    [[nodiscard]] static Microsoft::WRL::ComPtr<ID3D11BlendState> makeBlend(ID3D11Device& device, const Device::PipelineDesc& desc);
    [[nodiscard]] static Microsoft::WRL::ComPtr<ID3D11RasterizerState> makeRasterizer(ID3D11Device& device, const Device::PipelineDesc& desc);
    [[nodiscard]] static Microsoft::WRL::ComPtr<ID3D11DepthStencilState> makeDepthStencil(ID3D11Device& device, const Device::PipelineDesc& desc);
    [[nodiscard]] static Microsoft::WRL::ComPtr<ID3D11SamplerState> makeSampler(ID3D11Device& device, const Device::SamplerDesc& desc);
    [[nodiscard]] static D3D11_PRIMITIVE_TOPOLOGY toTopology(Device::Primitive primitive) noexcept;

  private:
    [[nodiscard]] static D3D11_BLEND toBlend(Device::BlendFactor factor) noexcept;
    [[nodiscard]] static D3D11_BLEND_OP toBlendOperation(Device::BlendOperation operation) noexcept;
    [[nodiscard]] static D3D11_COMPARISON_FUNC toCompare(Device::CompareFunction function) noexcept;
    [[nodiscard]] static D3D11_STENCIL_OP toStencilOperation(Device::StencilOperation operation) noexcept;
    [[nodiscard]] static D3D11_DEPTH_STENCILOP_DESC toStencilFace(const Device::StencilFace& face) noexcept;
    [[nodiscard]] static D3D11_TEXTURE_ADDRESS_MODE toAddress(Device::Wrap wrap) noexcept;
    [[nodiscard]] static D3D11_FILTER toFilter(const Device::SamplerDesc& desc) noexcept;
};

} // namespace haylen::gpu::d3d11
