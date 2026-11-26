#include "gpu/opengl/Conversions.hpp"

namespace haylen::gpu::opengl {

Conversions::Pixel Conversions::getPixel(Device::PixelFormat format, bool desktop) noexcept {
    using Format = Device::PixelFormat;
    switch (format) {
    case Format::R8:
        return {.internalFormat = Gl::kR8, .format = Gl::kRed};
    case Format::Rg8:
        return {.internalFormat = Gl::kRg8, .format = Gl::kRg};
    case Format::Rgba8:
        return {};
    case Format::Rgba8Srgb:
        return {.internalFormat = Gl::kSrgb8Alpha8, .srgb = true};
    case Format::Bgra8:
        return {.format = desktop ? Gl::kBgra : Gl::kRgba, .swapsRedBlue = !desktop};
    case Format::Rgb10A2:
        return {.internalFormat = Gl::kRgb10A2, .type = Gl::kUnsignedInt2101010Rev};
    case Format::Rg11B10Float:
        return {.internalFormat = Gl::kR11fG11fB10f, .format = Gl::kRgb, .type = Gl::kUnsignedInt10f11f11fRev, .packedFloating = true};
    case Format::R16Float:
        return {.internalFormat = Gl::kR16f, .format = Gl::kRed, .type = Gl::kHalfFloat, .halfFloating = true};
    case Format::Rg16Float:
        return {.internalFormat = Gl::kRg16f, .format = Gl::kRg, .type = Gl::kHalfFloat, .halfFloating = true};
    case Format::Rgba16Float:
        return {.internalFormat = Gl::kRgba16f, .type = Gl::kHalfFloat, .halfFloating = true};
    case Format::R32Float:
        return {.internalFormat = Gl::kR32f, .format = Gl::kRed, .type = Gl::kFloat, .floating = true};
    case Format::Rg32Float:
        return {.internalFormat = Gl::kRg32f, .format = Gl::kRg, .type = Gl::kFloat, .floating = true};
    case Format::Rgba32Float:
        return {.internalFormat = Gl::kRgba32f, .type = Gl::kFloat, .floating = true};
    case Format::Depth16:
        return {.internalFormat = Gl::kDepthComponent16, .format = Gl::kDepthComponent, .type = Gl::kUnsignedShort, .depth = true};
    case Format::Depth32Float:
        return {.internalFormat = Gl::kDepthComponent32f, .format = Gl::kDepthComponent, .type = Gl::kFloat, .depth = true};
    case Format::Depth24Stencil8:
        return {.internalFormat = Gl::kDepth24Stencil8, .format = Gl::kDepthStencil, .type = Gl::kUnsignedInt248, .depth = true, .stencil = true};
    case Format::Depth32FloatStencil8:
        return {.internalFormat = Gl::kDepth32fStencil8, .format = Gl::kDepthStencil, .type = Gl::kFloat32UnsignedInt248Rev, .depth = true, .stencil = true};
    case Format::Bc1:
        return {.internalFormat = Gl::kCompressedRgbaS3tcDxt1, .compressed = true};
    case Format::Bc1Srgb:
        return {.internalFormat = Gl::kCompressedSrgbAlphaS3tcDxt1, .compressed = true, .srgb = true};
    case Format::Bc2:
        return {.internalFormat = Gl::kCompressedRgbaS3tcDxt3, .compressed = true};
    case Format::Bc2Srgb:
        return {.internalFormat = Gl::kCompressedSrgbAlphaS3tcDxt3, .compressed = true, .srgb = true};
    case Format::Bc3:
        return {.internalFormat = Gl::kCompressedRgbaS3tcDxt5, .compressed = true};
    case Format::Bc3Srgb:
        return {.internalFormat = Gl::kCompressedSrgbAlphaS3tcDxt5, .compressed = true, .srgb = true};
    case Format::Bc4:
        return {.internalFormat = Gl::kCompressedRedRgtc1, .compressed = true};
    case Format::Bc4Snorm:
        return {.internalFormat = Gl::kCompressedSignedRedRgtc1, .compressed = true};
    case Format::Bc5:
        return {.internalFormat = Gl::kCompressedRgRgtc2, .compressed = true};
    case Format::Bc5Snorm:
        return {.internalFormat = Gl::kCompressedSignedRgRgtc2, .compressed = true};
    case Format::Bc6hUfloat:
        return {.internalFormat = Gl::kCompressedRgbBptcUnsignedFloat, .compressed = true};
    case Format::Bc6hFloat:
        return {.internalFormat = Gl::kCompressedRgbBptcSignedFloat, .compressed = true};
    case Format::Bc7:
        return {.internalFormat = Gl::kCompressedRgbaBptcUnorm, .compressed = true};
    case Format::Bc7Srgb:
        return {.internalFormat = Gl::kCompressedSrgbAlphaBptcUnorm, .compressed = true, .srgb = true};
    case Format::Etc2Rgb8:
        return {.internalFormat = Gl::kCompressedRgb8Etc2, .compressed = true};
    case Format::Etc2Rgb8Srgb:
        return {.internalFormat = Gl::kCompressedSrgb8Etc2, .compressed = true, .srgb = true};
    case Format::Etc2Rgb8A1:
        return {.internalFormat = Gl::kCompressedRgb8PunchthroughAlpha1Etc2, .compressed = true};
    case Format::Etc2Rgb8A1Srgb:
        return {.internalFormat = Gl::kCompressedSrgb8PunchthroughAlpha1Etc2, .compressed = true, .srgb = true};
    case Format::Etc2Rgba8:
        return {.internalFormat = Gl::kCompressedRgba8Etc2Eac, .compressed = true};
    case Format::Etc2Rgba8Srgb:
        return {.internalFormat = Gl::kCompressedSrgb8Alpha8Etc2Eac, .compressed = true, .srgb = true};
    case Format::EacR11:
        return {.internalFormat = Gl::kCompressedR11Eac, .compressed = true};
    case Format::EacR11Snorm:
        return {.internalFormat = Gl::kCompressedSignedR11Eac, .compressed = true};
    case Format::EacRg11:
        return {.internalFormat = Gl::kCompressedRg11Eac, .compressed = true};
    case Format::EacRg11Snorm:
        return {.internalFormat = Gl::kCompressedSignedRg11Eac, .compressed = true};
    case Format::Astc4x4:
    case Format::Astc4x4Hdr:
        return {.internalFormat = Gl::kCompressedRgbaAstcFirst + 0, .compressed = true};
    case Format::Astc4x4Srgb:
        return {.internalFormat = Gl::kCompressedSrgb8Alpha8AstcFirst + 0, .compressed = true, .srgb = true};
    case Format::Astc5x4:
    case Format::Astc5x4Hdr:
        return {.internalFormat = Gl::kCompressedRgbaAstcFirst + 1, .compressed = true};
    case Format::Astc5x4Srgb:
        return {.internalFormat = Gl::kCompressedSrgb8Alpha8AstcFirst + 1, .compressed = true, .srgb = true};
    case Format::Astc5x5:
    case Format::Astc5x5Hdr:
        return {.internalFormat = Gl::kCompressedRgbaAstcFirst + 2, .compressed = true};
    case Format::Astc5x5Srgb:
        return {.internalFormat = Gl::kCompressedSrgb8Alpha8AstcFirst + 2, .compressed = true, .srgb = true};
    case Format::Astc6x5:
    case Format::Astc6x5Hdr:
        return {.internalFormat = Gl::kCompressedRgbaAstcFirst + 3, .compressed = true};
    case Format::Astc6x5Srgb:
        return {.internalFormat = Gl::kCompressedSrgb8Alpha8AstcFirst + 3, .compressed = true, .srgb = true};
    case Format::Astc6x6:
    case Format::Astc6x6Hdr:
        return {.internalFormat = Gl::kCompressedRgbaAstcFirst + 4, .compressed = true};
    case Format::Astc6x6Srgb:
        return {.internalFormat = Gl::kCompressedSrgb8Alpha8AstcFirst + 4, .compressed = true, .srgb = true};
    case Format::Astc8x5:
    case Format::Astc8x5Hdr:
        return {.internalFormat = Gl::kCompressedRgbaAstcFirst + 5, .compressed = true};
    case Format::Astc8x5Srgb:
        return {.internalFormat = Gl::kCompressedSrgb8Alpha8AstcFirst + 5, .compressed = true, .srgb = true};
    case Format::Astc8x6:
    case Format::Astc8x6Hdr:
        return {.internalFormat = Gl::kCompressedRgbaAstcFirst + 6, .compressed = true};
    case Format::Astc8x6Srgb:
        return {.internalFormat = Gl::kCompressedSrgb8Alpha8AstcFirst + 6, .compressed = true, .srgb = true};
    case Format::Astc8x8:
    case Format::Astc8x8Hdr:
        return {.internalFormat = Gl::kCompressedRgbaAstcFirst + 7, .compressed = true};
    case Format::Astc8x8Srgb:
        return {.internalFormat = Gl::kCompressedSrgb8Alpha8AstcFirst + 7, .compressed = true, .srgb = true};
    case Format::Astc10x5:
    case Format::Astc10x5Hdr:
        return {.internalFormat = Gl::kCompressedRgbaAstcFirst + 8, .compressed = true};
    case Format::Astc10x5Srgb:
        return {.internalFormat = Gl::kCompressedSrgb8Alpha8AstcFirst + 8, .compressed = true, .srgb = true};
    case Format::Astc10x6:
    case Format::Astc10x6Hdr:
        return {.internalFormat = Gl::kCompressedRgbaAstcFirst + 9, .compressed = true};
    case Format::Astc10x6Srgb:
        return {.internalFormat = Gl::kCompressedSrgb8Alpha8AstcFirst + 9, .compressed = true, .srgb = true};
    case Format::Astc10x8:
    case Format::Astc10x8Hdr:
        return {.internalFormat = Gl::kCompressedRgbaAstcFirst + 10, .compressed = true};
    case Format::Astc10x8Srgb:
        return {.internalFormat = Gl::kCompressedSrgb8Alpha8AstcFirst + 10, .compressed = true, .srgb = true};
    case Format::Astc10x10:
    case Format::Astc10x10Hdr:
        return {.internalFormat = Gl::kCompressedRgbaAstcFirst + 11, .compressed = true};
    case Format::Astc10x10Srgb:
        return {.internalFormat = Gl::kCompressedSrgb8Alpha8AstcFirst + 11, .compressed = true, .srgb = true};
    case Format::Astc12x10:
    case Format::Astc12x10Hdr:
        return {.internalFormat = Gl::kCompressedRgbaAstcFirst + 12, .compressed = true};
    case Format::Astc12x10Srgb:
        return {.internalFormat = Gl::kCompressedSrgb8Alpha8AstcFirst + 12, .compressed = true, .srgb = true};
    case Format::Astc12x12:
    case Format::Astc12x12Hdr:
        return {.internalFormat = Gl::kCompressedRgbaAstcFirst + 13, .compressed = true};
    case Format::Astc12x12Srgb:
        return {.internalFormat = Gl::kCompressedSrgb8Alpha8AstcFirst + 13, .compressed = true, .srgb = true};
    }

    return {};
}

Conversions::Vertex Conversions::getVertex(Device::VertexFormat format) noexcept {
    using Format = Device::VertexFormat;
    switch (format) {
    case Format::Float:
        return {.size = 1};
    case Format::Float2:
        return {.size = 2};
    case Format::Float3:
        return {.size = 3};
    case Format::Float4:
        return {.size = 4};
    case Format::Half2:
        return {.size = 2, .type = Gl::kHalfFloat};
    case Format::Half4:
        return {.size = 4, .type = Gl::kHalfFloat};
    case Format::UByte4:
        return {.size = 4, .type = Gl::kUnsignedByte, .integer = true};
    case Format::UByte4Normalized:
        return {.size = 4, .type = Gl::kUnsignedByte, .normalized = true};
    case Format::Short2:
        return {.size = 2, .type = Gl::kShort, .integer = true};
    case Format::Short2Normalized:
        return {.size = 2, .type = Gl::kShort, .normalized = true};
    case Format::UShort4Normalized:
        return {.size = 4, .type = Gl::kUnsignedShort, .normalized = true};
    case Format::UInt:
        return {.size = 1, .type = Gl::kUnsignedInt, .integer = true};
    }

    return {};
}

Gl::Enum Conversions::toPrimitive(Device::Primitive primitive) noexcept {
    switch (primitive) {
    case Device::Primitive::Triangles:
        return Gl::kTriangles;
    case Device::Primitive::TriangleStrip:
        return Gl::kTriangleStrip;
    case Device::Primitive::Lines:
        return Gl::kLines;
    case Device::Primitive::LineStrip:
        return Gl::kLineStrip;
    case Device::Primitive::Points:
        return Gl::kPoints;
    }

    return Gl::kTriangles;
}

Gl::Enum Conversions::toIndexType(Device::IndexType type) noexcept {
    return type == Device::IndexType::UInt16 ? Gl::kUnsignedShort : Gl::kUnsignedInt;
}

std::size_t Conversions::getIndexSize(Device::IndexType type) noexcept {
    return type == Device::IndexType::UInt16 ? 2 : 4;
}

Gl::Enum Conversions::toBlendFactor(Device::BlendFactor factor) noexcept {
    using Factor = Device::BlendFactor;
    switch (factor) {
    case Factor::Zero:
        return Gl::kZero;
    case Factor::One:
        return Gl::kOne;
    case Factor::SourceColor:
        return Gl::kSourceColor;
    case Factor::OneMinusSourceColor:
        return Gl::kOneMinusSourceColor;
    case Factor::SourceAlpha:
        return Gl::kSourceAlpha;
    case Factor::OneMinusSourceAlpha:
        return Gl::kOneMinusSourceAlpha;
    case Factor::DestinationColor:
        return Gl::kDestinationColor;
    case Factor::OneMinusDestinationColor:
        return Gl::kOneMinusDestinationColor;
    case Factor::DestinationAlpha:
        return Gl::kDestinationAlpha;
    case Factor::OneMinusDestinationAlpha:
        return Gl::kOneMinusDestinationAlpha;
    case Factor::SourceAlphaSaturated:
        return Gl::kSourceAlphaSaturate;
    case Factor::BlendColor:
        return Gl::kConstantColor;
    case Factor::OneMinusBlendColor:
        return Gl::kOneMinusConstantColor;
    }

    return Gl::kOne;
}

Gl::Enum Conversions::toBlendOperation(Device::BlendOperation operation) noexcept {
    switch (operation) {
    case Device::BlendOperation::Add:
        return Gl::kFunctionAdd;
    case Device::BlendOperation::Subtract:
        return Gl::kFunctionSubtract;
    case Device::BlendOperation::ReverseSubtract:
        return Gl::kFunctionReverseSubtract;
    case Device::BlendOperation::Minimum:
        return Gl::kMinimum;
    case Device::BlendOperation::Maximum:
        return Gl::kMaximum;
    }

    return Gl::kFunctionAdd;
}

Gl::Enum Conversions::toCompare(Device::CompareFunction function) noexcept {
    using Function = Device::CompareFunction;
    switch (function) {
    case Function::Never:
        return Gl::kNever;
    case Function::Less:
        return Gl::kLess;
    case Function::Equal:
        return Gl::kEqual;
    case Function::LessEqual:
        return Gl::kLessEqual;
    case Function::Greater:
        return Gl::kGreater;
    case Function::NotEqual:
        return Gl::kNotEqual;
    case Function::GreaterEqual:
        return Gl::kGreaterEqual;
    case Function::Always:
        return Gl::kAlways;
    }

    return Gl::kAlways;
}

Gl::Enum Conversions::toStencilOperation(Device::StencilOperation operation) noexcept {
    using Operation = Device::StencilOperation;
    switch (operation) {
    case Operation::Keep:
        return Gl::kKeep;
    case Operation::Zero:
        return Gl::kZero;
    case Operation::Replace:
        return Gl::kReplace;
    case Operation::IncrementClamp:
        return Gl::kIncrement;
    case Operation::DecrementClamp:
        return Gl::kDecrement;
    case Operation::Invert:
        return Gl::kInvert;
    case Operation::IncrementWrap:
        return Gl::kIncrementWrap;
    case Operation::DecrementWrap:
        return Gl::kDecrementWrap;
    }

    return Gl::kKeep;
}

Gl::Enum Conversions::toMinification(Device::Filter minification, Device::Filter mipmap) noexcept {
    const bool linear = minification == Device::Filter::Linear;
    if (mipmap == Device::Filter::Linear) {
        return linear ? Gl::kLinearMipmapLinear : Gl::kNearestMipmapLinear;
    }

    return linear ? Gl::kLinearMipmapNearest : Gl::kNearestMipmapNearest;
}

Gl::Enum Conversions::toMagnification(Device::Filter magnification) noexcept {
    return magnification == Device::Filter::Linear ? Gl::kLinear : Gl::kNearest;
}

Gl::Enum Conversions::toWrap(Device::Wrap wrap) noexcept {
    switch (wrap) {
    case Device::Wrap::ClampToEdge:
        return Gl::kClampToEdge;
    case Device::Wrap::Repeat:
        return Gl::kRepeat;
    case Device::Wrap::MirroredRepeat:
        return Gl::kMirroredRepeat;
    case Device::Wrap::ClampToBorder:
        return Gl::kClampToBorder;
    }

    return Gl::kClampToEdge;
}

std::array<float, 4> Conversions::toBorderColor(Device::BorderColor color) noexcept {
    switch (color) {
    case Device::BorderColor::TransparentBlack:
        return {0.0F, 0.0F, 0.0F, 0.0F};
    case Device::BorderColor::OpaqueBlack:
        return {0.0F, 0.0F, 0.0F, 1.0F};
    case Device::BorderColor::OpaqueWhite:
        return {1.0F, 1.0F, 1.0F, 1.0F};
    }

    return {0.0F, 0.0F, 0.0F, 0.0F};
}

Gl::Enum Conversions::toTextureTarget(Device::TextureType type) noexcept {
    switch (type) {
    case Device::TextureType::Flat:
        return Gl::kTexture2d;
    case Device::TextureType::Array:
        return Gl::kTexture2dArray;
    case Device::TextureType::Volume:
        return Gl::kTexture3d;
    case Device::TextureType::Cube:
        return Gl::kTextureCubeMap;
    }

    return Gl::kTexture2d;
}

} // namespace haylen::gpu::opengl
