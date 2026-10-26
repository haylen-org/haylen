#include "gpu/metal/Formats.hpp"

namespace haylen::gpu::metal {

MTLPixelFormat Formats::toPixelFormat(gpu::Device::PixelFormat format, bool depth24Stencil8) noexcept {
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
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
        return depth24Stencil8 ? MTLPixelFormatDepth24Unorm_Stencil8 : MTLPixelFormatDepth32Float_Stencil8;
#else
        static_cast<void>(depth24Stencil8);
        return MTLPixelFormatDepth32Float_Stencil8;
#endif
    case PixelFormat::Depth32FloatStencil8:
        return MTLPixelFormatDepth32Float_Stencil8;
    }
    return MTLPixelFormatRGBA8Unorm;
}

std::size_t Formats::getBytesPerPixel(gpu::Device::PixelFormat format) noexcept {
    using PixelFormat = gpu::Device::PixelFormat;
    switch (format) {
    case PixelFormat::R8:
        return 1;
    case PixelFormat::Rg8:
    case PixelFormat::R16Float:
    case PixelFormat::Depth16:
        return 2;
    case PixelFormat::Rgba8:
    case PixelFormat::Rgba8Srgb:
    case PixelFormat::Bgra8:
    case PixelFormat::Rgb10A2:
    case PixelFormat::Rg16Float:
    case PixelFormat::R32Float:
    case PixelFormat::Depth32Float:
    case PixelFormat::Depth24Stencil8:
        return 4;
    case PixelFormat::Rgba16Float:
    case PixelFormat::Rg32Float:
    case PixelFormat::Depth32FloatStencil8:
        return 8;
    case PixelFormat::Rgba32Float:
        return 16;
    }
    return 4;
}

bool Formats::isDepth(gpu::Device::PixelFormat format) noexcept {
    using PixelFormat = gpu::Device::PixelFormat;
    return format == PixelFormat::Depth16 || format == PixelFormat::Depth32Float || format == PixelFormat::Depth24Stencil8 || format == PixelFormat::Depth32FloatStencil8;
}

bool Formats::hasStencil(gpu::Device::PixelFormat format) noexcept {
    using PixelFormat = gpu::Device::PixelFormat;
    return format == PixelFormat::Depth24Stencil8 || format == PixelFormat::Depth32FloatStencil8;
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
    }
    return MTLSamplerAddressModeClampToEdge;
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
