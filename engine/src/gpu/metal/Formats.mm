#include "gpu/metal/Formats.hpp"

namespace haylen::gpu::metal {

MTLPixelFormat Formats::toPixelFormat(gpu::Device::PixelFormat format) noexcept {
    using PixelFormat = gpu::Device::PixelFormat;
    switch (format) {
    case PixelFormat::R8:
        return MTLPixelFormatR8Unorm;
    case PixelFormat::Rg8:
        return MTLPixelFormatRG8Unorm;
    case PixelFormat::Rgba8:
        return MTLPixelFormatRGBA8Unorm;
    case PixelFormat::Rgba8Srgb:
        return MTLPixelFormatRGBA8Unorm_sRGB;
    case PixelFormat::Bgra8:
        return MTLPixelFormatBGRA8Unorm;
    case PixelFormat::Rgb10A2:
        return MTLPixelFormatRGB10A2Unorm;
    case PixelFormat::Rg11B10Float:
        return MTLPixelFormatRG11B10Float;
    case PixelFormat::R16Float:
        return MTLPixelFormatR16Float;
    case PixelFormat::Rg16Float:
        return MTLPixelFormatRG16Float;
    case PixelFormat::Rgba16Float:
        return MTLPixelFormatRGBA16Float;
    case PixelFormat::R32Float:
        return MTLPixelFormatR32Float;
    case PixelFormat::Rg32Float:
        return MTLPixelFormatRG32Float;
    case PixelFormat::Rgba32Float:
        return MTLPixelFormatRGBA32Float;
    case PixelFormat::Depth16:
        return MTLPixelFormatDepth16Unorm;
    case PixelFormat::Depth32Float:
        return MTLPixelFormatDepth32Float;
    case PixelFormat::Depth24Stencil8:
    case PixelFormat::Depth32FloatStencil8:
        return MTLPixelFormatDepth32Float_Stencil8;
    case PixelFormat::Bc1:
    case PixelFormat::Bc1Srgb:
    case PixelFormat::Bc2:
    case PixelFormat::Bc2Srgb:
    case PixelFormat::Bc3:
    case PixelFormat::Bc3Srgb:
    case PixelFormat::Bc4:
    case PixelFormat::Bc4Snorm:
    case PixelFormat::Bc5:
    case PixelFormat::Bc5Snorm:
    case PixelFormat::Bc6hUfloat:
    case PixelFormat::Bc6hFloat:
    case PixelFormat::Bc7:
    case PixelFormat::Bc7Srgb:
        if (@available(macOS 11.0, iOS 16.4, tvOS 16.4, *)) {
            return toBcPixelFormat(format);
        }

        return MTLPixelFormatInvalid;
    case PixelFormat::Etc2Rgb8:
        return MTLPixelFormatETC2_RGB8;
    case PixelFormat::Etc2Rgb8Srgb:
        return MTLPixelFormatETC2_RGB8_sRGB;
    case PixelFormat::Etc2Rgb8A1:
        return MTLPixelFormatETC2_RGB8A1;
    case PixelFormat::Etc2Rgb8A1Srgb:
        return MTLPixelFormatETC2_RGB8A1_sRGB;
    case PixelFormat::Etc2Rgba8:
        return MTLPixelFormatEAC_RGBA8;
    case PixelFormat::Etc2Rgba8Srgb:
        return MTLPixelFormatEAC_RGBA8_sRGB;
    case PixelFormat::EacR11:
        return MTLPixelFormatEAC_R11Unorm;
    case PixelFormat::EacR11Snorm:
        return MTLPixelFormatEAC_R11Snorm;
    case PixelFormat::EacRg11:
        return MTLPixelFormatEAC_RG11Unorm;
    case PixelFormat::EacRg11Snorm:
        return MTLPixelFormatEAC_RG11Snorm;
    case PixelFormat::Astc4x4:
        return MTLPixelFormatASTC_4x4_LDR;
    case PixelFormat::Astc4x4Srgb:
        return MTLPixelFormatASTC_4x4_sRGB;
    case PixelFormat::Astc4x4Hdr:
        return MTLPixelFormatASTC_4x4_HDR;
    case PixelFormat::Astc5x4:
        return MTLPixelFormatASTC_5x4_LDR;
    case PixelFormat::Astc5x4Srgb:
        return MTLPixelFormatASTC_5x4_sRGB;
    case PixelFormat::Astc5x4Hdr:
        return MTLPixelFormatASTC_5x4_HDR;
    case PixelFormat::Astc5x5:
        return MTLPixelFormatASTC_5x5_LDR;
    case PixelFormat::Astc5x5Srgb:
        return MTLPixelFormatASTC_5x5_sRGB;
    case PixelFormat::Astc5x5Hdr:
        return MTLPixelFormatASTC_5x5_HDR;
    case PixelFormat::Astc6x5:
        return MTLPixelFormatASTC_6x5_LDR;
    case PixelFormat::Astc6x5Srgb:
        return MTLPixelFormatASTC_6x5_sRGB;
    case PixelFormat::Astc6x5Hdr:
        return MTLPixelFormatASTC_6x5_HDR;
    case PixelFormat::Astc6x6:
        return MTLPixelFormatASTC_6x6_LDR;
    case PixelFormat::Astc6x6Srgb:
        return MTLPixelFormatASTC_6x6_sRGB;
    case PixelFormat::Astc6x6Hdr:
        return MTLPixelFormatASTC_6x6_HDR;
    case PixelFormat::Astc8x5:
        return MTLPixelFormatASTC_8x5_LDR;
    case PixelFormat::Astc8x5Srgb:
        return MTLPixelFormatASTC_8x5_sRGB;
    case PixelFormat::Astc8x5Hdr:
        return MTLPixelFormatASTC_8x5_HDR;
    case PixelFormat::Astc8x6:
        return MTLPixelFormatASTC_8x6_LDR;
    case PixelFormat::Astc8x6Srgb:
        return MTLPixelFormatASTC_8x6_sRGB;
    case PixelFormat::Astc8x6Hdr:
        return MTLPixelFormatASTC_8x6_HDR;
    case PixelFormat::Astc8x8:
        return MTLPixelFormatASTC_8x8_LDR;
    case PixelFormat::Astc8x8Srgb:
        return MTLPixelFormatASTC_8x8_sRGB;
    case PixelFormat::Astc8x8Hdr:
        return MTLPixelFormatASTC_8x8_HDR;
    case PixelFormat::Astc10x5:
        return MTLPixelFormatASTC_10x5_LDR;
    case PixelFormat::Astc10x5Srgb:
        return MTLPixelFormatASTC_10x5_sRGB;
    case PixelFormat::Astc10x5Hdr:
        return MTLPixelFormatASTC_10x5_HDR;
    case PixelFormat::Astc10x6:
        return MTLPixelFormatASTC_10x6_LDR;
    case PixelFormat::Astc10x6Srgb:
        return MTLPixelFormatASTC_10x6_sRGB;
    case PixelFormat::Astc10x6Hdr:
        return MTLPixelFormatASTC_10x6_HDR;
    case PixelFormat::Astc10x8:
        return MTLPixelFormatASTC_10x8_LDR;
    case PixelFormat::Astc10x8Srgb:
        return MTLPixelFormatASTC_10x8_sRGB;
    case PixelFormat::Astc10x8Hdr:
        return MTLPixelFormatASTC_10x8_HDR;
    case PixelFormat::Astc10x10:
        return MTLPixelFormatASTC_10x10_LDR;
    case PixelFormat::Astc10x10Srgb:
        return MTLPixelFormatASTC_10x10_sRGB;
    case PixelFormat::Astc10x10Hdr:
        return MTLPixelFormatASTC_10x10_HDR;
    case PixelFormat::Astc12x10:
        return MTLPixelFormatASTC_12x10_LDR;
    case PixelFormat::Astc12x10Srgb:
        return MTLPixelFormatASTC_12x10_sRGB;
    case PixelFormat::Astc12x10Hdr:
        return MTLPixelFormatASTC_12x10_HDR;
    case PixelFormat::Astc12x12:
        return MTLPixelFormatASTC_12x12_LDR;
    case PixelFormat::Astc12x12Srgb:
        return MTLPixelFormatASTC_12x12_sRGB;
    case PixelFormat::Astc12x12Hdr:
        return MTLPixelFormatASTC_12x12_HDR;
    }

    return MTLPixelFormatRGBA8Unorm;
}

MTLPixelFormat Formats::toBcPixelFormat(gpu::Device::PixelFormat format) noexcept {
    using PixelFormat = gpu::Device::PixelFormat;
    switch (format) {
    case PixelFormat::Bc1:
        return MTLPixelFormatBC1_RGBA;
    case PixelFormat::Bc1Srgb:
        return MTLPixelFormatBC1_RGBA_sRGB;
    case PixelFormat::Bc2:
        return MTLPixelFormatBC2_RGBA;
    case PixelFormat::Bc2Srgb:
        return MTLPixelFormatBC2_RGBA_sRGB;
    case PixelFormat::Bc3:
        return MTLPixelFormatBC3_RGBA;
    case PixelFormat::Bc3Srgb:
        return MTLPixelFormatBC3_RGBA_sRGB;
    case PixelFormat::Bc4:
        return MTLPixelFormatBC4_RUnorm;
    case PixelFormat::Bc4Snorm:
        return MTLPixelFormatBC4_RSnorm;
    case PixelFormat::Bc5:
        return MTLPixelFormatBC5_RGUnorm;
    case PixelFormat::Bc5Snorm:
        return MTLPixelFormatBC5_RGSnorm;
    case PixelFormat::Bc6hUfloat:
        return MTLPixelFormatBC6H_RGBUfloat;
    case PixelFormat::Bc6hFloat:
        return MTLPixelFormatBC6H_RGBFloat;
    case PixelFormat::Bc7:
        return MTLPixelFormatBC7_RGBAUnorm;
    case PixelFormat::Bc7Srgb:
        return MTLPixelFormatBC7_RGBAUnorm_sRGB;
    default:
        return MTLPixelFormatInvalid;
    }
}

MTLTextureType Formats::toTextureType(gpu::Device::TextureType type, bool multisampled) noexcept {
    using TextureType = gpu::Device::TextureType;
    switch (type) {
    case TextureType::Flat:
        return multisampled ? MTLTextureType2DMultisample : MTLTextureType2D;
    case TextureType::Array:
        return multisampled ? MTLTextureType2DMultisampleArray : MTLTextureType2DArray;
    case TextureType::Volume:
        return MTLTextureType3D;
    case TextureType::Cube:
        return MTLTextureTypeCube;
    case TextureType::CubeArray:
        return MTLTextureTypeCubeArray;
    }

    return MTLTextureType2D;
}

MTLVertexFormat Formats::toVertexFormat(gpu::Device::VertexFormat format) noexcept {
    using VertexFormat = gpu::Device::VertexFormat;
    switch (format) {
    case VertexFormat::Float:
        return MTLVertexFormatFloat;
    case VertexFormat::Float2:
        return MTLVertexFormatFloat2;
    case VertexFormat::Float3:
        return MTLVertexFormatFloat3;
    case VertexFormat::Float4:
        return MTLVertexFormatFloat4;
    case VertexFormat::Half2:
        return MTLVertexFormatHalf2;
    case VertexFormat::Half4:
        return MTLVertexFormatHalf4;
    case VertexFormat::UByte4:
        return MTLVertexFormatUChar4;
    case VertexFormat::UByte4Normalized:
        return MTLVertexFormatUChar4Normalized;
    case VertexFormat::Short2:
        return MTLVertexFormatShort2;
    case VertexFormat::Short2Normalized:
        return MTLVertexFormatShort2Normalized;
    case VertexFormat::UShort4Normalized:
        return MTLVertexFormatUShort4Normalized;
    case VertexFormat::UInt:
        return MTLVertexFormatUInt;
    }

    return MTLVertexFormatFloat;
}

MTLPrimitiveType Formats::toPrimitiveType(gpu::Device::Primitive primitive) noexcept {
    using Primitive = gpu::Device::Primitive;
    switch (primitive) {
    case Primitive::Triangles:
        return MTLPrimitiveTypeTriangle;
    case Primitive::TriangleStrip:
        return MTLPrimitiveTypeTriangleStrip;
    case Primitive::Lines:
        return MTLPrimitiveTypeLine;
    case Primitive::LineStrip:
        return MTLPrimitiveTypeLineStrip;
    case Primitive::Points:
        return MTLPrimitiveTypePoint;
    }

    return MTLPrimitiveTypeTriangle;
}

MTLPrimitiveTopologyClass Formats::toTopologyClass(gpu::Device::Primitive primitive) noexcept {
    using Primitive = gpu::Device::Primitive;
    switch (primitive) {
    case Primitive::Triangles:
    case Primitive::TriangleStrip:
        return MTLPrimitiveTopologyClassTriangle;
    case Primitive::Lines:
    case Primitive::LineStrip:
        return MTLPrimitiveTopologyClassLine;
    case Primitive::Points:
        return MTLPrimitiveTopologyClassPoint;
    }

    return MTLPrimitiveTopologyClassTriangle;
}

MTLBlendFactor Formats::toBlendFactor(gpu::Device::BlendFactor factor) noexcept {
    using BlendFactor = gpu::Device::BlendFactor;
    switch (factor) {
    case BlendFactor::Zero:
        return MTLBlendFactorZero;
    case BlendFactor::One:
        return MTLBlendFactorOne;
    case BlendFactor::SourceColor:
        return MTLBlendFactorSourceColor;
    case BlendFactor::OneMinusSourceColor:
        return MTLBlendFactorOneMinusSourceColor;
    case BlendFactor::SourceAlpha:
        return MTLBlendFactorSourceAlpha;
    case BlendFactor::OneMinusSourceAlpha:
        return MTLBlendFactorOneMinusSourceAlpha;
    case BlendFactor::DestinationColor:
        return MTLBlendFactorDestinationColor;
    case BlendFactor::OneMinusDestinationColor:
        return MTLBlendFactorOneMinusDestinationColor;
    case BlendFactor::DestinationAlpha:
        return MTLBlendFactorDestinationAlpha;
    case BlendFactor::OneMinusDestinationAlpha:
        return MTLBlendFactorOneMinusDestinationAlpha;
    case BlendFactor::SourceAlphaSaturated:
        return MTLBlendFactorSourceAlphaSaturated;
    case BlendFactor::BlendColor:
        return MTLBlendFactorBlendColor;
    case BlendFactor::OneMinusBlendColor:
        return MTLBlendFactorOneMinusBlendColor;
    }

    return MTLBlendFactorOne;
}

MTLBlendOperation Formats::toBlendOperation(gpu::Device::BlendOperation operation) noexcept {
    using BlendOperation = gpu::Device::BlendOperation;
    switch (operation) {
    case BlendOperation::Add:
        return MTLBlendOperationAdd;
    case BlendOperation::Subtract:
        return MTLBlendOperationSubtract;
    case BlendOperation::ReverseSubtract:
        return MTLBlendOperationReverseSubtract;
    case BlendOperation::Minimum:
        return MTLBlendOperationMin;
    case BlendOperation::Maximum:
        return MTLBlendOperationMax;
    }

    return MTLBlendOperationAdd;
}

MTLColorWriteMask Formats::toColorWriteMask(gpu::Device::ColorMask mask) noexcept {
    const auto bits = static_cast<unsigned>(mask);
    MTLColorWriteMask written = MTLColorWriteMaskNone;
    if ((bits & static_cast<unsigned>(gpu::Device::ColorMask::Red)) != 0U) {
        written |= MTLColorWriteMaskRed;
    }

    if ((bits & static_cast<unsigned>(gpu::Device::ColorMask::Green)) != 0U) {
        written |= MTLColorWriteMaskGreen;
    }

    if ((bits & static_cast<unsigned>(gpu::Device::ColorMask::Blue)) != 0U) {
        written |= MTLColorWriteMaskBlue;
    }

    if ((bits & static_cast<unsigned>(gpu::Device::ColorMask::Alpha)) != 0U) {
        written |= MTLColorWriteMaskAlpha;
    }

    return written;
}

MTLCompareFunction Formats::toCompareFunction(gpu::Device::CompareFunction function) noexcept {
    using CompareFunction = gpu::Device::CompareFunction;
    switch (function) {
    case CompareFunction::Never:
        return MTLCompareFunctionNever;
    case CompareFunction::Less:
        return MTLCompareFunctionLess;
    case CompareFunction::Equal:
        return MTLCompareFunctionEqual;
    case CompareFunction::LessEqual:
        return MTLCompareFunctionLessEqual;
    case CompareFunction::Greater:
        return MTLCompareFunctionGreater;
    case CompareFunction::NotEqual:
        return MTLCompareFunctionNotEqual;
    case CompareFunction::GreaterEqual:
        return MTLCompareFunctionGreaterEqual;
    case CompareFunction::Always:
        return MTLCompareFunctionAlways;
    }

    return MTLCompareFunctionAlways;
}

MTLStencilOperation Formats::toStencilOperation(gpu::Device::StencilOperation operation) noexcept {
    using StencilOperation = gpu::Device::StencilOperation;
    switch (operation) {
    case StencilOperation::Keep:
        return MTLStencilOperationKeep;
    case StencilOperation::Zero:
        return MTLStencilOperationZero;
    case StencilOperation::Replace:
        return MTLStencilOperationReplace;
    case StencilOperation::IncrementClamp:
        return MTLStencilOperationIncrementClamp;
    case StencilOperation::DecrementClamp:
        return MTLStencilOperationDecrementClamp;
    case StencilOperation::Invert:
        return MTLStencilOperationInvert;
    case StencilOperation::IncrementWrap:
        return MTLStencilOperationIncrementWrap;
    case StencilOperation::DecrementWrap:
        return MTLStencilOperationDecrementWrap;
    }

    return MTLStencilOperationKeep;
}

MTLCullMode Formats::toCullMode(gpu::Device::CullMode mode) noexcept {
    using CullMode = gpu::Device::CullMode;
    switch (mode) {
    case CullMode::None:
        return MTLCullModeNone;
    case CullMode::Front:
        return MTLCullModeFront;
    case CullMode::Back:
        return MTLCullModeBack;
    }

    return MTLCullModeNone;
}

MTLWinding Formats::toWinding(gpu::Device::FrontFace face) noexcept {
    return face == gpu::Device::FrontFace::Clockwise ? MTLWindingClockwise : MTLWindingCounterClockwise;
}

MTLSamplerMinMagFilter Formats::toFilter(gpu::Device::Filter filter) noexcept {
    return filter == gpu::Device::Filter::Nearest ? MTLSamplerMinMagFilterNearest : MTLSamplerMinMagFilterLinear;
}

MTLSamplerMipFilter Formats::toMipFilter(gpu::Device::Filter filter) noexcept {
    return filter == gpu::Device::Filter::Nearest ? MTLSamplerMipFilterNearest : MTLSamplerMipFilterLinear;
}

MTLSamplerAddressMode Formats::toAddressMode(gpu::Device::Wrap wrap) noexcept {
    using Wrap = gpu::Device::Wrap;
    switch (wrap) {
    case Wrap::ClampToEdge:
        return MTLSamplerAddressModeClampToEdge;
    case Wrap::Repeat:
        return MTLSamplerAddressModeRepeat;
    case Wrap::MirroredRepeat:
        return MTLSamplerAddressModeMirrorRepeat;
    case Wrap::ClampToBorder:
        return MTLSamplerAddressModeClampToBorderColor;
    }

    return MTLSamplerAddressModeClampToEdge;
}

MTLSamplerBorderColor Formats::toBorderColor(gpu::Device::BorderColor color) noexcept {
    using BorderColor = gpu::Device::BorderColor;
    switch (color) {
    case BorderColor::TransparentBlack:
        return MTLSamplerBorderColorTransparentBlack;
    case BorderColor::OpaqueBlack:
        return MTLSamplerBorderColorOpaqueBlack;
    case BorderColor::OpaqueWhite:
        return MTLSamplerBorderColorOpaqueWhite;
    }

    return MTLSamplerBorderColorTransparentBlack;
}

MTLLoadAction Formats::toLoadAction(gpu::Device::Load load) noexcept {
    using Load = gpu::Device::Load;
    switch (load) {
    case Load::Clear:
        return MTLLoadActionClear;
    case Load::Keep:
        return MTLLoadActionLoad;
    case Load::DontCare:
        return MTLLoadActionDontCare;
    }

    return MTLLoadActionClear;
}

MTLClearColor Formats::toClearColor(math::Color color) noexcept {
    return MTLClearColorMake(color.r, color.g, color.b, color.a);
}

} // namespace haylen::gpu::metal
