#pragma once

#include <webgpu/webgpu.h>

#include <string_view>

#include "haylen/gpu/Device.hpp"
#include "haylen/math/Color.hpp"

namespace haylen::gpu::webgpu {

// The values of the WebGPU C API that the values of the RHI stand for.
class Conversion final {
  public:
    Conversion() = delete;

    [[nodiscard]] static WGPUStringView toText(std::string_view text) noexcept;
    [[nodiscard]] static std::string_view fromText(WGPUStringView text) noexcept;
    [[nodiscard]] static WGPUTextureFormat toTextureFormat(Device::PixelFormat format) noexcept;
    [[nodiscard]] static WGPUVertexFormat toVertexFormat(Device::VertexFormat format) noexcept;
    [[nodiscard]] static WGPUPrimitiveTopology toTopology(Device::Primitive primitive) noexcept;
    [[nodiscard]] static WGPUIndexFormat toIndexFormat(Device::IndexType type) noexcept;
    [[nodiscard]] static WGPUBlendFactor toBlendFactor(Device::BlendFactor factor) noexcept;
    [[nodiscard]] static WGPUBlendOperation toBlendOperation(Device::BlendOperation operation) noexcept;
    [[nodiscard]] static WGPUBlendComponent toBlendComponent(Device::BlendFactor source, Device::BlendFactor destination, Device::BlendOperation operation) noexcept;
    [[nodiscard]] static WGPUColorWriteMask toWriteMask(Device::ColorMask mask) noexcept;
    [[nodiscard]] static WGPUCompareFunction toCompare(Device::CompareFunction function) noexcept;
    [[nodiscard]] static WGPUStencilOperation toStencilOperation(Device::StencilOperation operation) noexcept;
    [[nodiscard]] static WGPUStencilFaceState toStencilFace(const Device::StencilFace& face) noexcept;
    [[nodiscard]] static WGPUCullMode toCullMode(Device::CullMode mode) noexcept;
    [[nodiscard]] static WGPUFrontFace toFrontFace(Device::FrontFace face) noexcept;
    [[nodiscard]] static WGPUFilterMode toFilter(Device::Filter filter) noexcept;
    [[nodiscard]] static WGPUMipmapFilterMode toMipmapFilter(Device::Filter filter) noexcept;
    [[nodiscard]] static WGPUAddressMode toAddressMode(Device::Wrap wrap) noexcept;
    [[nodiscard]] static WGPUShaderStage toStage(Device::Stage stage) noexcept;
    [[nodiscard]] static WGPUTextureDimension toDimension(Device::TextureType type) noexcept;
    [[nodiscard]] static WGPUTextureViewDimension toViewDimension(Device::TextureType type) noexcept;
    [[nodiscard]] static WGPUTextureSampleType toSampleType(Device::SampleType type) noexcept;
    [[nodiscard]] static WGPUSamplerBindingType toSamplerType(Device::SamplerType type) noexcept;
    [[nodiscard]] static WGPULoadOp toLoad(Device::Load load) noexcept;
    [[nodiscard]] static WGPUStoreOp toStore(Device::Store store) noexcept;
    [[nodiscard]] static WGPUColor toColor(math::Color color) noexcept;
};

} // namespace haylen::gpu::webgpu
