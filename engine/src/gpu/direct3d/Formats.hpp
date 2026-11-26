#pragma once

#include <dxgiformat.h>

#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::direct3d {

// The DXGI formats of the pixel, vertex and index formats of the devices of Direct3D 11 and 12. Depth images are typeless resources, so a depth stencil view writes them and a shader resource view samples their depth.
class Formats final {
  public:
    using PixelFormat = Device::PixelFormat;

    // The swapchain holds 8-bit colors in the order that the composition of Windows reads without a conversion, with a depth and stencil image of its own.
    static constexpr DXGI_FORMAT kSwapchainColor = DXGI_FORMAT_B8G8R8A8_UNORM;
    static constexpr PixelFormat kSwapchainColorFormat = PixelFormat::Bgra8;
    static constexpr PixelFormat kSwapchainDepthFormat = PixelFormat::Depth24Stencil8;

    [[nodiscard]] static DXGI_FORMAT getResourceFormat(PixelFormat format) noexcept;

    // The format that samples the image, which is `DXGI_FORMAT_UNKNOWN` for the ETC2, EAC and ASTC formats that Direct3D has none of, so the devices of Direct3D report them unsupported.
    [[nodiscard]] static DXGI_FORMAT getViewFormat(PixelFormat format) noexcept;
    [[nodiscard]] static DXGI_FORMAT getDepthFormat(PixelFormat format) noexcept;
    [[nodiscard]] static DXGI_FORMAT getVertexFormat(Device::VertexFormat format) noexcept;
    [[nodiscard]] static DXGI_FORMAT getIndexFormat(Device::IndexType type) noexcept;
};

} // namespace haylen::gpu::direct3d
