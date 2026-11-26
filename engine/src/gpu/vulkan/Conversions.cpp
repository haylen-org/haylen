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
    case Device::PixelFormat::Rg11B10Float:
        return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    case Device::PixelFormat::Bc1:
        return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
    case Device::PixelFormat::Bc1Srgb:
        return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
    case Device::PixelFormat::Bc2:
        return VK_FORMAT_BC2_UNORM_BLOCK;
    case Device::PixelFormat::Bc2Srgb:
        return VK_FORMAT_BC2_SRGB_BLOCK;
    case Device::PixelFormat::Bc3:
        return VK_FORMAT_BC3_UNORM_BLOCK;
    case Device::PixelFormat::Bc3Srgb:
        return VK_FORMAT_BC3_SRGB_BLOCK;
    case Device::PixelFormat::Bc4:
        return VK_FORMAT_BC4_UNORM_BLOCK;
    case Device::PixelFormat::Bc4Snorm:
        return VK_FORMAT_BC4_SNORM_BLOCK;
    case Device::PixelFormat::Bc5:
        return VK_FORMAT_BC5_UNORM_BLOCK;
    case Device::PixelFormat::Bc5Snorm:
        return VK_FORMAT_BC5_SNORM_BLOCK;
    case Device::PixelFormat::Bc6hUfloat:
        return VK_FORMAT_BC6H_UFLOAT_BLOCK;
    case Device::PixelFormat::Bc6hFloat:
        return VK_FORMAT_BC6H_SFLOAT_BLOCK;
    case Device::PixelFormat::Bc7:
        return VK_FORMAT_BC7_UNORM_BLOCK;
    case Device::PixelFormat::Bc7Srgb:
        return VK_FORMAT_BC7_SRGB_BLOCK;
    case Device::PixelFormat::Etc2Rgb8:
        return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
    case Device::PixelFormat::Etc2Rgb8Srgb:
        return VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK;
    case Device::PixelFormat::Etc2Rgb8A1:
        return VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK;
    case Device::PixelFormat::Etc2Rgb8A1Srgb:
        return VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK;
    case Device::PixelFormat::Etc2Rgba8:
        return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
    case Device::PixelFormat::Etc2Rgba8Srgb:
        return VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK;
    case Device::PixelFormat::EacR11:
        return VK_FORMAT_EAC_R11_UNORM_BLOCK;
    case Device::PixelFormat::EacR11Snorm:
        return VK_FORMAT_EAC_R11_SNORM_BLOCK;
    case Device::PixelFormat::EacRg11:
        return VK_FORMAT_EAC_R11G11_UNORM_BLOCK;
    case Device::PixelFormat::EacRg11Snorm:
        return VK_FORMAT_EAC_R11G11_SNORM_BLOCK;
    case Device::PixelFormat::Astc4x4:
        return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
    case Device::PixelFormat::Astc4x4Srgb:
        return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
    case Device::PixelFormat::Astc4x4Hdr:
        return VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK;
    case Device::PixelFormat::Astc5x4:
        return VK_FORMAT_ASTC_5x4_UNORM_BLOCK;
    case Device::PixelFormat::Astc5x4Srgb:
        return VK_FORMAT_ASTC_5x4_SRGB_BLOCK;
    case Device::PixelFormat::Astc5x4Hdr:
        return VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK;
    case Device::PixelFormat::Astc5x5:
        return VK_FORMAT_ASTC_5x5_UNORM_BLOCK;
    case Device::PixelFormat::Astc5x5Srgb:
        return VK_FORMAT_ASTC_5x5_SRGB_BLOCK;
    case Device::PixelFormat::Astc5x5Hdr:
        return VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK;
    case Device::PixelFormat::Astc6x5:
        return VK_FORMAT_ASTC_6x5_UNORM_BLOCK;
    case Device::PixelFormat::Astc6x5Srgb:
        return VK_FORMAT_ASTC_6x5_SRGB_BLOCK;
    case Device::PixelFormat::Astc6x5Hdr:
        return VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK;
    case Device::PixelFormat::Astc6x6:
        return VK_FORMAT_ASTC_6x6_UNORM_BLOCK;
    case Device::PixelFormat::Astc6x6Srgb:
        return VK_FORMAT_ASTC_6x6_SRGB_BLOCK;
    case Device::PixelFormat::Astc6x6Hdr:
        return VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK;
    case Device::PixelFormat::Astc8x5:
        return VK_FORMAT_ASTC_8x5_UNORM_BLOCK;
    case Device::PixelFormat::Astc8x5Srgb:
        return VK_FORMAT_ASTC_8x5_SRGB_BLOCK;
    case Device::PixelFormat::Astc8x5Hdr:
        return VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK;
    case Device::PixelFormat::Astc8x6:
        return VK_FORMAT_ASTC_8x6_UNORM_BLOCK;
    case Device::PixelFormat::Astc8x6Srgb:
        return VK_FORMAT_ASTC_8x6_SRGB_BLOCK;
    case Device::PixelFormat::Astc8x6Hdr:
        return VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK;
    case Device::PixelFormat::Astc8x8:
        return VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
    case Device::PixelFormat::Astc8x8Srgb:
        return VK_FORMAT_ASTC_8x8_SRGB_BLOCK;
    case Device::PixelFormat::Astc8x8Hdr:
        return VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK;
    case Device::PixelFormat::Astc10x5:
        return VK_FORMAT_ASTC_10x5_UNORM_BLOCK;
    case Device::PixelFormat::Astc10x5Srgb:
        return VK_FORMAT_ASTC_10x5_SRGB_BLOCK;
    case Device::PixelFormat::Astc10x5Hdr:
        return VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK;
    case Device::PixelFormat::Astc10x6:
        return VK_FORMAT_ASTC_10x6_UNORM_BLOCK;
    case Device::PixelFormat::Astc10x6Srgb:
        return VK_FORMAT_ASTC_10x6_SRGB_BLOCK;
    case Device::PixelFormat::Astc10x6Hdr:
        return VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK;
    case Device::PixelFormat::Astc10x8:
        return VK_FORMAT_ASTC_10x8_UNORM_BLOCK;
    case Device::PixelFormat::Astc10x8Srgb:
        return VK_FORMAT_ASTC_10x8_SRGB_BLOCK;
    case Device::PixelFormat::Astc10x8Hdr:
        return VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK;
    case Device::PixelFormat::Astc10x10:
        return VK_FORMAT_ASTC_10x10_UNORM_BLOCK;
    case Device::PixelFormat::Astc10x10Srgb:
        return VK_FORMAT_ASTC_10x10_SRGB_BLOCK;
    case Device::PixelFormat::Astc10x10Hdr:
        return VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK;
    case Device::PixelFormat::Astc12x10:
        return VK_FORMAT_ASTC_12x10_UNORM_BLOCK;
    case Device::PixelFormat::Astc12x10Srgb:
        return VK_FORMAT_ASTC_12x10_SRGB_BLOCK;
    case Device::PixelFormat::Astc12x10Hdr:
        return VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK;
    case Device::PixelFormat::Astc12x12:
        return VK_FORMAT_ASTC_12x12_UNORM_BLOCK;
    case Device::PixelFormat::Astc12x12Srgb:
        return VK_FORMAT_ASTC_12x12_SRGB_BLOCK;
    case Device::PixelFormat::Astc12x12Hdr:
        return VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK;
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
    case Device::Wrap::ClampToBorder:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    }

    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
}

VkBorderColor Conversions::toBorderColor(Device::BorderColor color) noexcept {
    switch (color) {
    case Device::BorderColor::TransparentBlack:
        return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    case Device::BorderColor::OpaqueBlack:
        return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    case Device::BorderColor::OpaqueWhite:
        return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    }

    return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
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

VkBufferUsageFlags Conversions::toBufferUsage(const Device::BufferUsage& usage) noexcept {
    VkBufferUsageFlags flags = 0;
    if (usage.vertex) {
        flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }

    if (usage.index) {
        flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }

    if (usage.uniform) {
        flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }

    if (usage.storage) {
        flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }

    if (usage.indirect) {
        flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    }

    if (usage.copySource) {
        flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    }

    if (usage.copyDestination) {
        flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    return flags;
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
    case VK_INCOMPLETE:
        return "VK_INCOMPLETE";
    case VK_ERROR_MEMORY_MAP_FAILED:
        return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_TOO_MANY_OBJECTS:
        return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FRAGMENTATION:
        return "VK_ERROR_FRAGMENTATION";
    case VK_ERROR_INVALID_EXTERNAL_HANDLE:
        return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
    case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
        return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
    case VK_PIPELINE_COMPILE_REQUIRED:
        return "VK_PIPELINE_COMPILE_REQUIRED";
    case VK_ERROR_UNKNOWN:
        return "VK_ERROR_UNKNOWN";
    default:
        return "an unknown result";
    }
}

} // namespace haylen::gpu::vulkan
