#include "gpu/opengl/Conversions.hpp"

namespace haylen::gpu::opengl {

Conversions::Pixel Conversions::getPixel(Device::PixelFormat format, bool desktop) noexcept {
    using Format = Device::PixelFormat;
    switch (format) {
    case Format::R8:
        return {.internalFormat = Gl::kR8, .format = Gl::kRed, .bytes = 1};
    case Format::Rg8:
        return {.internalFormat = Gl::kRg8, .format = Gl::kRg, .bytes = 2};
    case Format::Rgba8:
        return {};
    case Format::Rgba8Srgb:
        return {.internalFormat = Gl::kSrgb8Alpha8, .srgb = true};
    case Format::Bgra8:
        return {.format = desktop ? Gl::kBgra : Gl::kRgba, .swapsRedBlue = !desktop};
    case Format::Rgb10A2:
        return {.internalFormat = Gl::kRgb10A2, .type = Gl::kUnsignedInt2101010Rev};
    case Format::R16Float:
        return {.internalFormat = Gl::kR16f, .format = Gl::kRed, .type = Gl::kHalfFloat, .bytes = 2, .halfFloating = true};
    case Format::Rg16Float:
        return {.internalFormat = Gl::kRg16f, .format = Gl::kRg, .type = Gl::kHalfFloat, .bytes = 4, .halfFloating = true};
    case Format::Rgba16Float:
        return {.internalFormat = Gl::kRgba16f, .type = Gl::kHalfFloat, .bytes = 8, .halfFloating = true};
    case Format::R32Float:
        return {.internalFormat = Gl::kR32f, .format = Gl::kRed, .type = Gl::kFloat, .bytes = 4, .floating = true};
    case Format::Rg32Float:
        return {.internalFormat = Gl::kRg32f, .format = Gl::kRg, .type = Gl::kFloat, .bytes = 8, .floating = true};
    case Format::Rgba32Float:
        return {.internalFormat = Gl::kRgba32f, .type = Gl::kFloat, .bytes = 16, .floating = true};
    case Format::Depth16:
        return {.internalFormat = Gl::kDepthComponent16, .format = Gl::kDepthComponent, .type = Gl::kUnsignedShort, .bytes = 2, .depth = true};
    case Format::Depth32Float:
        return {.internalFormat = Gl::kDepthComponent32f, .format = Gl::kDepthComponent, .type = Gl::kFloat, .bytes = 4, .depth = true};
    case Format::Depth24Stencil8:
        return {.internalFormat = Gl::kDepth24Stencil8, .format = Gl::kDepthStencil, .type = Gl::kUnsignedInt248, .bytes = 4, .depth = true, .stencil = true};
    case Format::Depth32FloatStencil8:
        return {.internalFormat = Gl::kDepth32fStencil8, .format = Gl::kDepthStencil, .type = Gl::kFloat32UnsignedInt248Rev, .bytes = 8, .depth = true, .stencil = true};
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
    }
    return Gl::kClampToEdge;
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
