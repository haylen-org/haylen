#pragma once

#include <bit>
#include <format>
#include <optional>
#include <stdexcept>
#include <string_view>

#include "haylen/gpu/Surface.hpp"

namespace haylen::gpu::direct3d {

// The samples that the swapchain of a window of Direct3D 11 or 12 draws with: a power of two up to the most that the GPU takes for the color and the depth of the swapchain together, which the device checks before it makes any swapchain, so a count the GPU cannot take fails with a sentence instead of an error code of Direct3D.
class WindowSamples final {
  public:
    WindowSamples() = delete;

    // Throws `std::runtime_error` for a window whose sample count the GPU cannot take, and accepts a device without a window.
    static void check(const std::optional<Surface>& window, int most, std::string_view api) {
        if (!window) {
            return;
        }

        const int samples = window->sampleCount;
        if (samples < 1 || samples > most || !std::has_single_bit(static_cast<unsigned>(samples))) {
            throw std::runtime_error(std::format("The window asks for {} samples, and {} draws a window on this GPU with 1, 2, 4 or 8 samples, up to {}.", samples, api, most));
        }
    }
};

} // namespace haylen::gpu::direct3d
