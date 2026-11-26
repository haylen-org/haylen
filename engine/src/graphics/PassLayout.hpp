#pragma once

#include "haylen/gpu/Device.hpp"

namespace haylen::graphics {

// The format and the sample count of the color that an offscreen pass draws into, which the pipelines of its draws take. The images of the renderers and every swapchain pass take the format of offscreen images with one sample, since a pipeline of the swapchain takes the attachments of the swapchain.
struct PassLayout {
    gpu::Device::PixelFormat color = gpu::Device::PixelFormat::Rgba8;
    int samples = 1;

    friend bool operator==(const PassLayout&, const PassLayout&) = default;
};

} // namespace haylen::graphics
