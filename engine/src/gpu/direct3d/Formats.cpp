#include "gpu/direct3d/Formats.hpp"

namespace haylen::gpu::direct3d {

DXGI_FORMAT Formats::getResourceFormat(PixelFormat format) noexcept {
    switch (format) {
    case PixelFormat::Depth16:
        return DXGI_FORMAT_R16_TYPELESS;
    case PixelFormat::Depth32Float:
        return DXGI_FORMAT_R32_TYPELESS;
    case PixelFormat::Depth24Stencil8:
        return DXGI_FORMAT_R24G8_TYPELESS;
    case PixelFormat::Depth32FloatStencil8:
        return DXGI_FORMAT_R32G8X24_TYPELESS;
    default:
        return getViewFormat(format);
    }
}

DXGI_FORMAT Formats::getViewFormat(PixelFormat format) noexcept {
    switch (format) {
    case PixelFormat::R8:
        return DXGI_FORMAT_R8_UNORM;
    case PixelFormat::Rg8:
        return DXGI_FORMAT_R8G8_UNORM;
    case PixelFormat::Rgba8:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case PixelFormat::Rgba8Srgb:
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case PixelFormat::Bgra8:
        return DXGI_FORMAT_B8G8R8A8_UNORM;
    case PixelFormat::Rgb10A2:
        return DXGI_FORMAT_R10G10B10A2_UNORM;
    case PixelFormat::R16Float:
        return DXGI_FORMAT_R16_FLOAT;
    case PixelFormat::Rg16Float:
        return DXGI_FORMAT_R16G16_FLOAT;
    case PixelFormat::Rgba16Float:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case PixelFormat::R32Float:
        return DXGI_FORMAT_R32_FLOAT;
    case PixelFormat::Rg32Float:
        return DXGI_FORMAT_R32G32_FLOAT;
    case PixelFormat::Rgba32Float:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case PixelFormat::Depth16:
        return DXGI_FORMAT_R16_UNORM;
    case PixelFormat::Depth32Float:
        return DXGI_FORMAT_R32_FLOAT;
    case PixelFormat::Depth24Stencil8:
        return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    case PixelFormat::Depth32FloatStencil8:
        return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
    case PixelFormat::Rg11B10Float:
        return DXGI_FORMAT_R11G11B10_FLOAT;
    case PixelFormat::Bc1:
        return DXGI_FORMAT_BC1_UNORM;
    case PixelFormat::Bc1Srgb:
        return DXGI_FORMAT_BC1_UNORM_SRGB;
    case PixelFormat::Bc2:
        return DXGI_FORMAT_BC2_UNORM;
    case PixelFormat::Bc2Srgb:
        return DXGI_FORMAT_BC2_UNORM_SRGB;
    case PixelFormat::Bc3:
        return DXGI_FORMAT_BC3_UNORM;
    case PixelFormat::Bc3Srgb:
        return DXGI_FORMAT_BC3_UNORM_SRGB;
    case PixelFormat::Bc4:
        return DXGI_FORMAT_BC4_UNORM;
    case PixelFormat::Bc4Snorm:
        return DXGI_FORMAT_BC4_SNORM;
    case PixelFormat::Bc5:
        return DXGI_FORMAT_BC5_UNORM;
    case PixelFormat::Bc5Snorm:
        return DXGI_FORMAT_BC5_SNORM;
    case PixelFormat::Bc6hUfloat:
        return DXGI_FORMAT_BC6H_UF16;
    case PixelFormat::Bc6hFloat:
        return DXGI_FORMAT_BC6H_SF16;
    case PixelFormat::Bc7:
        return DXGI_FORMAT_BC7_UNORM;
    case PixelFormat::Bc7Srgb:
        return DXGI_FORMAT_BC7_UNORM_SRGB;
    default:
        return DXGI_FORMAT_UNKNOWN;
    }
}

DXGI_FORMAT Formats::getDepthFormat(PixelFormat format) noexcept {
    switch (format) {
    case PixelFormat::Depth16:
        return DXGI_FORMAT_D16_UNORM;
    case PixelFormat::Depth32Float:
        return DXGI_FORMAT_D32_FLOAT;
    case PixelFormat::Depth24Stencil8:
        return DXGI_FORMAT_D24_UNORM_S8_UINT;
    case PixelFormat::Depth32FloatStencil8:
        return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
    default:
        return DXGI_FORMAT_UNKNOWN;
    }
}

std::array<float, 4> Formats::getBorderColor(Device::BorderColor color) noexcept {
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

DXGI_FORMAT Formats::getVertexFormat(Device::VertexFormat format) noexcept {
    using VertexFormat = Device::VertexFormat;
    switch (format) {
    case VertexFormat::Float:
        return DXGI_FORMAT_R32_FLOAT;
    case VertexFormat::Float2:
        return DXGI_FORMAT_R32G32_FLOAT;
    case VertexFormat::Float3:
        return DXGI_FORMAT_R32G32B32_FLOAT;
    case VertexFormat::Float4:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case VertexFormat::Half2:
        return DXGI_FORMAT_R16G16_FLOAT;
    case VertexFormat::Half4:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case VertexFormat::UByte4:
        return DXGI_FORMAT_R8G8B8A8_UINT;
    case VertexFormat::UByte4Normalized:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case VertexFormat::Short2:
        return DXGI_FORMAT_R16G16_SINT;
    case VertexFormat::Short2Normalized:
        return DXGI_FORMAT_R16G16_SNORM;
    case VertexFormat::UShort4Normalized:
        return DXGI_FORMAT_R16G16B16A16_UNORM;
    case VertexFormat::UInt:
        return DXGI_FORMAT_R32_UINT;
    }

    return DXGI_FORMAT_UNKNOWN;
}

DXGI_FORMAT Formats::getIndexFormat(Device::IndexType type) noexcept {
    switch (type) {
    case Device::IndexType::UInt16:
        return DXGI_FORMAT_R16_UINT;
    case Device::IndexType::UInt32:
        return DXGI_FORMAT_R32_UINT;
    case Device::IndexType::None:
        break;
    }

    return DXGI_FORMAT_UNKNOWN;
}

} // namespace haylen::gpu::direct3d
