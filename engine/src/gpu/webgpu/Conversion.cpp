#include "gpu/webgpu/Conversion.hpp"

namespace haylen::gpu::webgpu {

WGPUStringView Conversion::toText(std::string_view text) noexcept {
    return {.data = text.data(), .length = text.size()};
}

std::string_view Conversion::fromText(WGPUStringView text) noexcept {
    if (text.data == nullptr) {
        return {};
    }
    return text.length == WGPU_STRLEN ? std::string_view(text.data) : std::string_view(text.data, text.length);
}

WGPUTextureFormat Conversion::toTextureFormat(Device::PixelFormat format) noexcept {
    switch (format) {
    case Device::PixelFormat::R8:
        return WGPUTextureFormat_R8Unorm;
    case Device::PixelFormat::Rg8:
        return WGPUTextureFormat_RG8Unorm;
    case Device::PixelFormat::Rgba8:
        return WGPUTextureFormat_RGBA8Unorm;
    case Device::PixelFormat::Rgba8Srgb:
        return WGPUTextureFormat_RGBA8UnormSrgb;
    case Device::PixelFormat::Bgra8:
        return WGPUTextureFormat_BGRA8Unorm;
    case Device::PixelFormat::Rgb10A2:
        return WGPUTextureFormat_RGB10A2Unorm;
    case Device::PixelFormat::R16Float:
        return WGPUTextureFormat_R16Float;
    case Device::PixelFormat::Rg16Float:
        return WGPUTextureFormat_RG16Float;
    case Device::PixelFormat::Rgba16Float:
        return WGPUTextureFormat_RGBA16Float;
    case Device::PixelFormat::R32Float:
        return WGPUTextureFormat_R32Float;
    case Device::PixelFormat::Rg32Float:
        return WGPUTextureFormat_RG32Float;
    case Device::PixelFormat::Rgba32Float:
        return WGPUTextureFormat_RGBA32Float;
    case Device::PixelFormat::Depth16:
        return WGPUTextureFormat_Depth16Unorm;
    case Device::PixelFormat::Depth32Float:
        return WGPUTextureFormat_Depth32Float;
    case Device::PixelFormat::Depth24Stencil8:
        return WGPUTextureFormat_Depth24PlusStencil8;
    case Device::PixelFormat::Depth32FloatStencil8:
        return WGPUTextureFormat_Depth32FloatStencil8;
    }
    return WGPUTextureFormat_RGBA8Unorm;
}

WGPUVertexFormat Conversion::toVertexFormat(Device::VertexFormat format) noexcept {
    switch (format) {
    case Device::VertexFormat::Float:
        return WGPUVertexFormat_Float32;
    case Device::VertexFormat::Float2:
        return WGPUVertexFormat_Float32x2;
    case Device::VertexFormat::Float3:
        return WGPUVertexFormat_Float32x3;
    case Device::VertexFormat::Float4:
        return WGPUVertexFormat_Float32x4;
    case Device::VertexFormat::Half2:
        return WGPUVertexFormat_Float16x2;
    case Device::VertexFormat::Half4:
        return WGPUVertexFormat_Float16x4;
    case Device::VertexFormat::UByte4:
        return WGPUVertexFormat_Uint8x4;
    case Device::VertexFormat::UByte4Normalized:
        return WGPUVertexFormat_Unorm8x4;
    case Device::VertexFormat::Short2:
        return WGPUVertexFormat_Sint16x2;
    case Device::VertexFormat::Short2Normalized:
        return WGPUVertexFormat_Snorm16x2;
    case Device::VertexFormat::UShort4Normalized:
        return WGPUVertexFormat_Unorm16x4;
    case Device::VertexFormat::UInt:
        return WGPUVertexFormat_Uint32;
    }
    return WGPUVertexFormat_Float32;
}

WGPUPrimitiveTopology Conversion::toTopology(Device::Primitive primitive) noexcept {
    switch (primitive) {
    case Device::Primitive::Triangles:
        return WGPUPrimitiveTopology_TriangleList;
    case Device::Primitive::TriangleStrip:
        return WGPUPrimitiveTopology_TriangleStrip;
    case Device::Primitive::Lines:
        return WGPUPrimitiveTopology_LineList;
    case Device::Primitive::LineStrip:
        return WGPUPrimitiveTopology_LineStrip;
    case Device::Primitive::Points:
        return WGPUPrimitiveTopology_PointList;
    }
    return WGPUPrimitiveTopology_TriangleList;
}

WGPUIndexFormat Conversion::toIndexFormat(Device::IndexType type) noexcept {
    switch (type) {
    case Device::IndexType::None:
        return WGPUIndexFormat_Undefined;
    case Device::IndexType::UInt16:
        return WGPUIndexFormat_Uint16;
    case Device::IndexType::UInt32:
        return WGPUIndexFormat_Uint32;
    }
    return WGPUIndexFormat_Undefined;
}

WGPUBlendFactor Conversion::toBlendFactor(Device::BlendFactor factor) noexcept {
    switch (factor) {
    case Device::BlendFactor::Zero:
        return WGPUBlendFactor_Zero;
    case Device::BlendFactor::One:
        return WGPUBlendFactor_One;
    case Device::BlendFactor::SourceColor:
        return WGPUBlendFactor_Src;
    case Device::BlendFactor::OneMinusSourceColor:
        return WGPUBlendFactor_OneMinusSrc;
    case Device::BlendFactor::SourceAlpha:
        return WGPUBlendFactor_SrcAlpha;
    case Device::BlendFactor::OneMinusSourceAlpha:
        return WGPUBlendFactor_OneMinusSrcAlpha;
    case Device::BlendFactor::DestinationColor:
        return WGPUBlendFactor_Dst;
    case Device::BlendFactor::OneMinusDestinationColor:
        return WGPUBlendFactor_OneMinusDst;
    case Device::BlendFactor::DestinationAlpha:
        return WGPUBlendFactor_DstAlpha;
    case Device::BlendFactor::OneMinusDestinationAlpha:
        return WGPUBlendFactor_OneMinusDstAlpha;
    case Device::BlendFactor::SourceAlphaSaturated:
        return WGPUBlendFactor_SrcAlphaSaturated;
    case Device::BlendFactor::BlendColor:
        return WGPUBlendFactor_Constant;
    case Device::BlendFactor::OneMinusBlendColor:
        return WGPUBlendFactor_OneMinusConstant;
    }
    return WGPUBlendFactor_One;
}

WGPUBlendOperation Conversion::toBlendOperation(Device::BlendOperation operation) noexcept {
    switch (operation) {
    case Device::BlendOperation::Add:
        return WGPUBlendOperation_Add;
    case Device::BlendOperation::Subtract:
        return WGPUBlendOperation_Subtract;
    case Device::BlendOperation::ReverseSubtract:
        return WGPUBlendOperation_ReverseSubtract;
    case Device::BlendOperation::Minimum:
        return WGPUBlendOperation_Min;
    case Device::BlendOperation::Maximum:
        return WGPUBlendOperation_Max;
    }
    return WGPUBlendOperation_Add;
}

// The minimum and the maximum ignore their factors, which WebGPU requires to be one for them.
WGPUBlendComponent Conversion::toBlendComponent(Device::BlendFactor source, Device::BlendFactor destination, Device::BlendOperation operation) noexcept {
    if (operation == Device::BlendOperation::Minimum || operation == Device::BlendOperation::Maximum) {
        return {.operation = toBlendOperation(operation), .srcFactor = WGPUBlendFactor_One, .dstFactor = WGPUBlendFactor_One};
    }
    return {.operation = toBlendOperation(operation), .srcFactor = toBlendFactor(source), .dstFactor = toBlendFactor(destination)};
}

WGPUColorWriteMask Conversion::toWriteMask(Device::ColorMask mask) noexcept {
    const auto bits = static_cast<unsigned>(mask);
    WGPUColorWriteMask result = WGPUColorWriteMask_None;
    if ((bits & static_cast<unsigned>(Device::ColorMask::Red)) != 0) {
        result |= WGPUColorWriteMask_Red;
    }
    if ((bits & static_cast<unsigned>(Device::ColorMask::Green)) != 0) {
        result |= WGPUColorWriteMask_Green;
    }
    if ((bits & static_cast<unsigned>(Device::ColorMask::Blue)) != 0) {
        result |= WGPUColorWriteMask_Blue;
    }
    if ((bits & static_cast<unsigned>(Device::ColorMask::Alpha)) != 0) {
        result |= WGPUColorWriteMask_Alpha;
    }
    return result;
}

WGPUCompareFunction Conversion::toCompare(Device::CompareFunction function) noexcept {
    switch (function) {
    case Device::CompareFunction::Never:
        return WGPUCompareFunction_Never;
    case Device::CompareFunction::Less:
        return WGPUCompareFunction_Less;
    case Device::CompareFunction::Equal:
        return WGPUCompareFunction_Equal;
    case Device::CompareFunction::LessEqual:
        return WGPUCompareFunction_LessEqual;
    case Device::CompareFunction::Greater:
        return WGPUCompareFunction_Greater;
    case Device::CompareFunction::NotEqual:
        return WGPUCompareFunction_NotEqual;
    case Device::CompareFunction::GreaterEqual:
        return WGPUCompareFunction_GreaterEqual;
    case Device::CompareFunction::Always:
        return WGPUCompareFunction_Always;
    }
    return WGPUCompareFunction_Always;
}

WGPUStencilOperation Conversion::toStencilOperation(Device::StencilOperation operation) noexcept {
    switch (operation) {
    case Device::StencilOperation::Keep:
        return WGPUStencilOperation_Keep;
    case Device::StencilOperation::Zero:
        return WGPUStencilOperation_Zero;
    case Device::StencilOperation::Replace:
        return WGPUStencilOperation_Replace;
    case Device::StencilOperation::IncrementClamp:
        return WGPUStencilOperation_IncrementClamp;
    case Device::StencilOperation::DecrementClamp:
        return WGPUStencilOperation_DecrementClamp;
    case Device::StencilOperation::Invert:
        return WGPUStencilOperation_Invert;
    case Device::StencilOperation::IncrementWrap:
        return WGPUStencilOperation_IncrementWrap;
    case Device::StencilOperation::DecrementWrap:
        return WGPUStencilOperation_DecrementWrap;
    }
    return WGPUStencilOperation_Keep;
}

WGPUStencilFaceState Conversion::toStencilFace(const Device::StencilFace& face) noexcept {
    return {.compare = toCompare(face.compare), .failOp = toStencilOperation(face.fail), .depthFailOp = toStencilOperation(face.depthFail), .passOp = toStencilOperation(face.pass)};
}

WGPUCullMode Conversion::toCullMode(Device::CullMode mode) noexcept {
    switch (mode) {
    case Device::CullMode::None:
        return WGPUCullMode_None;
    case Device::CullMode::Front:
        return WGPUCullMode_Front;
    case Device::CullMode::Back:
        return WGPUCullMode_Back;
    }
    return WGPUCullMode_None;
}

WGPUFrontFace Conversion::toFrontFace(Device::FrontFace face) noexcept {
    return face == Device::FrontFace::Clockwise ? WGPUFrontFace_CW : WGPUFrontFace_CCW;
}

WGPUFilterMode Conversion::toFilter(Device::Filter filter) noexcept {
    return filter == Device::Filter::Nearest ? WGPUFilterMode_Nearest : WGPUFilterMode_Linear;
}

WGPUMipmapFilterMode Conversion::toMipmapFilter(Device::Filter filter) noexcept {
    return filter == Device::Filter::Nearest ? WGPUMipmapFilterMode_Nearest : WGPUMipmapFilterMode_Linear;
}

WGPUAddressMode Conversion::toAddressMode(Device::Wrap wrap) noexcept {
    switch (wrap) {
    case Device::Wrap::ClampToEdge:
        return WGPUAddressMode_ClampToEdge;
    case Device::Wrap::Repeat:
        return WGPUAddressMode_Repeat;
    case Device::Wrap::MirroredRepeat:
        return WGPUAddressMode_MirrorRepeat;
    }
    return WGPUAddressMode_ClampToEdge;
}

WGPUShaderStage Conversion::toStage(Device::Stage stage) noexcept {
    return stage == Device::Stage::Vertex ? WGPUShaderStage_Vertex : WGPUShaderStage_Fragment;
}

WGPUTextureDimension Conversion::toDimension(Device::TextureType type) noexcept {
    return type == Device::TextureType::Volume ? WGPUTextureDimension_3D : WGPUTextureDimension_2D;
}

WGPUTextureViewDimension Conversion::toViewDimension(Device::TextureType type) noexcept {
    switch (type) {
    case Device::TextureType::Flat:
        return WGPUTextureViewDimension_2D;
    case Device::TextureType::Array:
        return WGPUTextureViewDimension_2DArray;
    case Device::TextureType::Volume:
        return WGPUTextureViewDimension_3D;
    case Device::TextureType::Cube:
        return WGPUTextureViewDimension_Cube;
    }
    return WGPUTextureViewDimension_2D;
}

WGPUTextureSampleType Conversion::toSampleType(Device::SampleType type) noexcept {
    switch (type) {
    case Device::SampleType::Float:
        return WGPUTextureSampleType_Float;
    case Device::SampleType::UnfilterableFloat:
        return WGPUTextureSampleType_UnfilterableFloat;
    case Device::SampleType::Depth:
        return WGPUTextureSampleType_Depth;
    case Device::SampleType::SignedInteger:
        return WGPUTextureSampleType_Sint;
    case Device::SampleType::UnsignedInteger:
        return WGPUTextureSampleType_Uint;
    }
    return WGPUTextureSampleType_Float;
}

WGPUSamplerBindingType Conversion::toSamplerType(Device::SamplerType type) noexcept {
    switch (type) {
    case Device::SamplerType::Filtering:
        return WGPUSamplerBindingType_Filtering;
    case Device::SamplerType::NonFiltering:
        return WGPUSamplerBindingType_NonFiltering;
    case Device::SamplerType::Comparison:
        return WGPUSamplerBindingType_Comparison;
    }
    return WGPUSamplerBindingType_Filtering;
}

// WebGPU has no load action that leaves the contents undefined, and clearing is what its tile GPUs do fastest.
WGPULoadOp Conversion::toLoad(Device::Load load) noexcept {
    return load == Device::Load::Keep ? WGPULoadOp_Load : WGPULoadOp_Clear;
}

WGPUStoreOp Conversion::toStore(Device::Store store) noexcept {
    return store == Device::Store::Store ? WGPUStoreOp_Store : WGPUStoreOp_Discard;
}

WGPUColor Conversion::toColor(math::Color color) noexcept {
    return {.r = color.r, .g = color.g, .b = color.b, .a = color.a};
}

} // namespace haylen::gpu::webgpu
