#include "gpu/vulkan/Conversions.hpp"

namespace haylen::gpu::vulkan {

VkFormat Conversions::toFormat(Device::PixelFormat format) noexcept {
    switch (format) {
    case Device::PixelFormat::R8:
        return VK_FORMAT_R8_UNORM;
    case Device::PixelFormat::Rg8:
        return VK_FORMAT_R8G8_UNORM;
    case Device::PixelFormat::Rgba8:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case Device::PixelFormat::Rgba8Srgb:
        return VK_FORMAT_R8G8B8A8_SRGB;
    case Device::PixelFormat::Bgra8:
        return VK_FORMAT_B8G8R8A8_UNORM;
    case Device::PixelFormat::Rgb10A2:
        return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    case Device::PixelFormat::R16Float:
        return VK_FORMAT_R16_SFLOAT;
    case Device::PixelFormat::Rg16Float:
        return VK_FORMAT_R16G16_SFLOAT;
    case Device::PixelFormat::Rgba16Float:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case Device::PixelFormat::R32Float:
        return VK_FORMAT_R32_SFLOAT;
    case Device::PixelFormat::Rg32Float:
        return VK_FORMAT_R32G32_SFLOAT;
    case Device::PixelFormat::Rgba32Float:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case Device::PixelFormat::Depth16:
        return VK_FORMAT_D16_UNORM;
    case Device::PixelFormat::Depth32Float:
        return VK_FORMAT_D32_SFLOAT;
    case Device::PixelFormat::Depth24Stencil8:
        return VK_FORMAT_D24_UNORM_S8_UINT;
    case Device::PixelFormat::Depth32FloatStencil8:
        return VK_FORMAT_D32_SFLOAT_S8_UINT;
    }
    return VK_FORMAT_UNDEFINED;
}

VkFormat Conversions::toFormat(Device::VertexFormat format) noexcept {
    switch (format) {
    case Device::VertexFormat::Float:
        return VK_FORMAT_R32_SFLOAT;
    case Device::VertexFormat::Float2:
        return VK_FORMAT_R32G32_SFLOAT;
    case Device::VertexFormat::Float3:
        return VK_FORMAT_R32G32B32_SFLOAT;
    case Device::VertexFormat::Float4:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case Device::VertexFormat::Half2:
        return VK_FORMAT_R16G16_SFLOAT;
    case Device::VertexFormat::Half4:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case Device::VertexFormat::UByte4:
        return VK_FORMAT_R8G8B8A8_UINT;
    case Device::VertexFormat::UByte4Normalized:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case Device::VertexFormat::Short2:
        return VK_FORMAT_R16G16_SINT;
    case Device::VertexFormat::Short2Normalized:
        return VK_FORMAT_R16G16_SNORM;
    case Device::VertexFormat::UShort4Normalized:
        return VK_FORMAT_R16G16B16A16_UNORM;
    case Device::VertexFormat::UInt:
        return VK_FORMAT_R32_UINT;
    }
    return VK_FORMAT_UNDEFINED;
}

std::size_t Conversions::getBytesPerPixel(Device::PixelFormat format) noexcept {
    switch (format) {
    case Device::PixelFormat::R8:
        return 1;
    case Device::PixelFormat::Rg8:
    case Device::PixelFormat::R16Float:
    case Device::PixelFormat::Depth16:
        return 2;
    case Device::PixelFormat::Rgba8:
    case Device::PixelFormat::Rgba8Srgb:
    case Device::PixelFormat::Bgra8:
    case Device::PixelFormat::Rgb10A2:
    case Device::PixelFormat::Rg16Float:
    case Device::PixelFormat::R32Float:
    case Device::PixelFormat::Depth32Float:
    case Device::PixelFormat::Depth24Stencil8:
        return 4;
    case Device::PixelFormat::Rgba16Float:
    case Device::PixelFormat::Rg32Float:
    case Device::PixelFormat::Depth32FloatStencil8:
        return 8;
    case Device::PixelFormat::Rgba32Float:
        return 16;
    }
    return 4;
}

bool Conversions::isDepth(Device::PixelFormat format) noexcept {
    return format == Device::PixelFormat::Depth16 || format == Device::PixelFormat::Depth32Float || format == Device::PixelFormat::Depth24Stencil8 || format == Device::PixelFormat::Depth32FloatStencil8;
}

bool Conversions::hasStencil(VkFormat format) noexcept {
    return format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D32_SFLOAT_S8_UINT;
}

VkImageAspectFlags Conversions::getAspects(VkFormat format) noexcept {
    switch (format) {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D32_SFLOAT:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    default:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

VkPrimitiveTopology Conversions::toTopology(Device::Primitive primitive) noexcept {
    switch (primitive) {
    case Device::Primitive::Triangles:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case Device::Primitive::TriangleStrip:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    case Device::Primitive::Lines:
        return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case Device::Primitive::LineStrip:
        return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case Device::Primitive::Points:
        return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    }
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

VkBlendFactor Conversions::toBlendFactor(Device::BlendFactor factor) noexcept {
    switch (factor) {
    case Device::BlendFactor::Zero:
        return VK_BLEND_FACTOR_ZERO;
    case Device::BlendFactor::One:
        return VK_BLEND_FACTOR_ONE;
    case Device::BlendFactor::SourceColor:
        return VK_BLEND_FACTOR_SRC_COLOR;
    case Device::BlendFactor::OneMinusSourceColor:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case Device::BlendFactor::SourceAlpha:
        return VK_BLEND_FACTOR_SRC_ALPHA;
    case Device::BlendFactor::OneMinusSourceAlpha:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case Device::BlendFactor::DestinationColor:
        return VK_BLEND_FACTOR_DST_COLOR;
    case Device::BlendFactor::OneMinusDestinationColor:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case Device::BlendFactor::DestinationAlpha:
        return VK_BLEND_FACTOR_DST_ALPHA;
    case Device::BlendFactor::OneMinusDestinationAlpha:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case Device::BlendFactor::SourceAlphaSaturated:
        return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
    case Device::BlendFactor::BlendColor:
        return VK_BLEND_FACTOR_CONSTANT_COLOR;
    case Device::BlendFactor::OneMinusBlendColor:
        return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
    }
    return VK_BLEND_FACTOR_ONE;
}

VkBlendOp Conversions::toBlendOperation(Device::BlendOperation operation) noexcept {
    switch (operation) {
    case Device::BlendOperation::Add:
        return VK_BLEND_OP_ADD;
    case Device::BlendOperation::Subtract:
        return VK_BLEND_OP_SUBTRACT;
    case Device::BlendOperation::ReverseSubtract:
        return VK_BLEND_OP_REVERSE_SUBTRACT;
    case Device::BlendOperation::Minimum:
        return VK_BLEND_OP_MIN;
    case Device::BlendOperation::Maximum:
        return VK_BLEND_OP_MAX;
    }
    return VK_BLEND_OP_ADD;
}

// The bits of the mask of the contract are the bits of Vulkan in the same order.
VkColorComponentFlags Conversions::toColorMask(Device::ColorMask mask) noexcept {
    return static_cast<VkColorComponentFlags>(mask);
}

VkCompareOp Conversions::toCompare(Device::CompareFunction function) noexcept {
    switch (function) {
    case Device::CompareFunction::Never:
        return VK_COMPARE_OP_NEVER;
    case Device::CompareFunction::Less:
        return VK_COMPARE_OP_LESS;
    case Device::CompareFunction::Equal:
        return VK_COMPARE_OP_EQUAL;
    case Device::CompareFunction::LessEqual:
        return VK_COMPARE_OP_LESS_OR_EQUAL;
    case Device::CompareFunction::Greater:
        return VK_COMPARE_OP_GREATER;
    case Device::CompareFunction::NotEqual:
        return VK_COMPARE_OP_NOT_EQUAL;
    case Device::CompareFunction::GreaterEqual:
        return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case Device::CompareFunction::Always:
        return VK_COMPARE_OP_ALWAYS;
    }
    return VK_COMPARE_OP_ALWAYS;
}

VkStencilOp Conversions::toStencilOperation(Device::StencilOperation operation) noexcept {
    switch (operation) {
    case Device::StencilOperation::Keep:
        return VK_STENCIL_OP_KEEP;
    case Device::StencilOperation::Zero:
        return VK_STENCIL_OP_ZERO;
    case Device::StencilOperation::Replace:
        return VK_STENCIL_OP_REPLACE;
    case Device::StencilOperation::IncrementClamp:
        return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
    case Device::StencilOperation::DecrementClamp:
        return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
    case Device::StencilOperation::Invert:
        return VK_STENCIL_OP_INVERT;
    case Device::StencilOperation::IncrementWrap:
        return VK_STENCIL_OP_INCREMENT_AND_WRAP;
    case Device::StencilOperation::DecrementWrap:
        return VK_STENCIL_OP_DECREMENT_AND_WRAP;
    }
    return VK_STENCIL_OP_KEEP;
}

VkCullModeFlags Conversions::toCullMode(Device::CullMode mode) noexcept {
    switch (mode) {
    case Device::CullMode::None:
        return VK_CULL_MODE_NONE;
    case Device::CullMode::Front:
        return VK_CULL_MODE_FRONT_BIT;
    case Device::CullMode::Back:
        return VK_CULL_MODE_BACK_BIT;
    }
    return VK_CULL_MODE_NONE;
}

// The viewport of the device flips the vertical axis, so the winding that the screen shows is the one Vulkan reads, as on Metal and Direct3D.
VkFrontFace Conversions::toFrontFace(Device::FrontFace face) noexcept {
    return face == Device::FrontFace::Clockwise ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
}

VkFilter Conversions::toFilter(Device::Filter filter) noexcept {
    return filter == Device::Filter::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
}

VkSamplerMipmapMode Conversions::toMipmapMode(Device::Filter filter) noexcept {
    return filter == Device::Filter::Nearest ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;
}

VkSamplerAddressMode Conversions::toAddressMode(Device::Wrap wrap) noexcept {
    switch (wrap) {
    case Device::Wrap::ClampToEdge:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case Device::Wrap::Repeat:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case Device::Wrap::MirroredRepeat:
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    }
    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
}

VkShaderStageFlags Conversions::toStage(Device::Stage stage) noexcept {
    return stage == Device::Stage::Vertex ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
}

VkImageViewType Conversions::toViewType(Device::TextureType type) noexcept {
    switch (type) {
    case Device::TextureType::Flat:
        return VK_IMAGE_VIEW_TYPE_2D;
    case Device::TextureType::Array:
        return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    case Device::TextureType::Volume:
        return VK_IMAGE_VIEW_TYPE_3D;
    case Device::TextureType::Cube:
        return VK_IMAGE_VIEW_TYPE_CUBE;
    }
    return VK_IMAGE_VIEW_TYPE_2D;
}

VkAttachmentLoadOp Conversions::toLoad(Device::Load load) noexcept {
    switch (load) {
    case Device::Load::Clear:
        return VK_ATTACHMENT_LOAD_OP_CLEAR;
    case Device::Load::Keep:
        return VK_ATTACHMENT_LOAD_OP_LOAD;
    case Device::Load::DontCare:
        return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
    return VK_ATTACHMENT_LOAD_OP_CLEAR;
}

VkAttachmentStoreOp Conversions::toStore(Device::Store store) noexcept {
    return store == Device::Store::Store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

VkSampleCountFlagBits Conversions::toSampleCount(int count) noexcept {
    switch (count) {
    case 2:
        return VK_SAMPLE_COUNT_2_BIT;
    case 4:
        return VK_SAMPLE_COUNT_4_BIT;
    case 8:
        return VK_SAMPLE_COUNT_8_BIT;
    case 16:
        return VK_SAMPLE_COUNT_16_BIT;
    default:
        return VK_SAMPLE_COUNT_1_BIT;
    }
}

VkIndexType Conversions::toIndexType(Device::IndexType type) noexcept {
    return type == Device::IndexType::UInt32 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
}

std::size_t Conversions::getIndexSize(Device::IndexType type) noexcept {
    return type == Device::IndexType::UInt32 ? 4 : 2;
}

std::string_view Conversions::describe(VkResult result) noexcept {
    switch (result) {
    case VK_SUCCESS:
        return "VK_SUCCESS";
    case VK_NOT_READY:
        return "VK_NOT_READY";
    case VK_TIMEOUT:
        return "VK_TIMEOUT";
    case VK_SUBOPTIMAL_KHR:
        return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED:
        return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST:
        return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_LAYER_NOT_PRESENT:
        return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT:
        return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT:
        return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER:
        return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
        return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL:
        return "VK_ERROR_FRAGMENTED_POOL";
    case VK_ERROR_OUT_OF_POOL_MEMORY:
        return "VK_ERROR_OUT_OF_POOL_MEMORY";
    case VK_ERROR_SURFACE_LOST_KHR:
        return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
        return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR:
        return "VK_ERROR_OUT_OF_DATE_KHR";
    default:
        return "an unknown result";
    }
}

} // namespace haylen::gpu::vulkan
