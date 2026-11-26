#pragma once

#import <Metal/Metal.h>

#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::metal {

// The values of Metal that stand for the enumerations of the device contract, and the facts about pixel formats that the device needs.
class Formats final {
  public:
    Formats() = delete;

    // The format of the image, where `Depth24Stencil8` takes `Depth32FloatStencil8`, the depth and stencil format of every Metal GPU.
    [[nodiscard]] static MTLPixelFormat toPixelFormat(gpu::Device::PixelFormat format) noexcept;

    // The formats of the BC family, which Metal has on iOS and tvOS from 16.4 on.
    [[nodiscard]] static MTLPixelFormat toBcPixelFormat(gpu::Device::PixelFormat format) noexcept API_AVAILABLE(macos(11.0), ios(16.4), tvos(16.4));

    [[nodiscard]] static MTLTextureType toTextureType(gpu::Device::TextureType type, bool multisampled) noexcept;
    [[nodiscard]] static MTLVertexFormat toVertexFormat(gpu::Device::VertexFormat format) noexcept;
    [[nodiscard]] static MTLPrimitiveType toPrimitiveType(gpu::Device::Primitive primitive) noexcept;
    [[nodiscard]] static MTLPrimitiveTopologyClass toTopologyClass(gpu::Device::Primitive primitive) noexcept;
    [[nodiscard]] static MTLBlendFactor toBlendFactor(gpu::Device::BlendFactor factor) noexcept;
    [[nodiscard]] static MTLBlendOperation toBlendOperation(gpu::Device::BlendOperation operation) noexcept;
    [[nodiscard]] static MTLColorWriteMask toColorWriteMask(gpu::Device::ColorMask mask) noexcept;
    [[nodiscard]] static MTLCompareFunction toCompareFunction(gpu::Device::CompareFunction function) noexcept;
    [[nodiscard]] static MTLStencilOperation toStencilOperation(gpu::Device::StencilOperation operation) noexcept;
    [[nodiscard]] static MTLCullMode toCullMode(gpu::Device::CullMode mode) noexcept;
    [[nodiscard]] static MTLWinding toWinding(gpu::Device::FrontFace face) noexcept;
    [[nodiscard]] static MTLSamplerMinMagFilter toFilter(gpu::Device::Filter filter) noexcept;
    [[nodiscard]] static MTLSamplerMipFilter toMipFilter(gpu::Device::Filter filter) noexcept;
    [[nodiscard]] static MTLSamplerAddressMode toAddressMode(gpu::Device::Wrap wrap) noexcept;
    [[nodiscard]] static MTLSamplerBorderColor toBorderColor(gpu::Device::BorderColor color) noexcept;
    [[nodiscard]] static MTLLoadAction toLoadAction(gpu::Device::Load load) noexcept;
    [[nodiscard]] static MTLMultisampleDepthResolveFilter toDepthResolveFilter(gpu::Device::DepthResolve mode) noexcept;
    [[nodiscard]] static MTLClearColor toClearColor(math::Color color) noexcept;
};

} // namespace haylen::gpu::metal
