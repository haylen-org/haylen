#pragma once

#include <d3d12.h>

#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::d3d12 {

// The descriptions of Direct3D 12 that the pipelines and samplers of the device turn into, which pipeline state objects and sampler descriptors take as they are.
class StateDescriptions final {
  public:
    [[nodiscard]] static D3D12_BLEND_DESC makeBlend(const Device::PipelineDesc& desc) noexcept;
    [[nodiscard]] static D3D12_RASTERIZER_DESC makeRasterizer(const Device::PipelineDesc& desc) noexcept;
    [[nodiscard]] static D3D12_DEPTH_STENCIL_DESC makeDepthStencil(const Device::PipelineDesc& desc) noexcept;
    [[nodiscard]] static D3D12_SAMPLER_DESC makeSampler(const Device::SamplerDesc& desc) noexcept;
    [[nodiscard]] static D3D12_PRIMITIVE_TOPOLOGY toTopology(Device::Primitive primitive) noexcept;
    [[nodiscard]] static D3D12_PRIMITIVE_TOPOLOGY_TYPE toTopologyType(Device::Primitive primitive) noexcept;

  private:
    [[nodiscard]] static D3D12_BLEND toBlend(Device::BlendFactor factor) noexcept;
    [[nodiscard]] static D3D12_BLEND toAlphaBlend(Device::BlendFactor factor) noexcept;
    [[nodiscard]] static D3D12_BLEND_OP toBlendOperation(Device::BlendOperation operation) noexcept;
    [[nodiscard]] static D3D12_COMPARISON_FUNC toCompare(Device::CompareFunction function) noexcept;
    [[nodiscard]] static D3D12_STENCIL_OP toStencilOperation(Device::StencilOperation operation) noexcept;
    [[nodiscard]] static D3D12_DEPTH_STENCILOP_DESC toStencilFace(const Device::StencilFace& face) noexcept;
    [[nodiscard]] static D3D12_TEXTURE_ADDRESS_MODE toAddress(Device::Wrap wrap) noexcept;
    [[nodiscard]] static D3D12_FILTER toFilter(const Device::SamplerDesc& desc) noexcept;
};

} // namespace haylen::gpu::d3d12
