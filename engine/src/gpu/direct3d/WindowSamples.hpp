#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "haylen/gpu/Device.hpp"
#include "haylen/gpu/Surface.hpp"

namespace haylen::gpu::direct3d {

// The samples that the swapchain of a window of Direct3D 11 or 12 draws with: a count that the GPU takes for both the color and the depth of the swapchain, which the device checks before it makes any swapchain, so a count the GPU cannot take fails with a sentence instead of an error code of Direct3D.
class WindowSamples final {
  public:
    WindowSamples() = delete;

    // The sample counts, as the bits of `Capabilities::sampleCounts`, that both the color and the depth of the swapchain take.
    [[nodiscard]] static std::uint8_t getSwapchainCounts(const Device::Capabilities& capabilities) noexcept {
        const std::uint8_t color = capabilities.getFormat(Device::PixelFormat::Bgra8).sampleCounts;
        const std::uint8_t depth = capabilities.getFormat(Device::PixelFormat::Depth24Stencil8).sampleCounts;
        return static_cast<std::uint8_t>(color & depth);
    }

    // Throws `std::runtime_error` for a window whose sample count is not one of the counts, and accepts a device without a window.
    static void check(const std::optional<Surface>& window, std::uint8_t counts, std::string_view api) {
        if (!window) {
            return;
        }

        const int samples = window->sampleCount;
        if (samples >= 1 && samples <= 128 && (counts & static_cast<unsigned>(samples)) != 0 && (samples & (samples - 1)) == 0) {
            return;
        }

        throw std::runtime_error(std::format("The window asks for {} samples, and {} draws a window on this GPU with {} samples.", samples, api, describe(counts)));
    }

  private:
    // The counts as words of a sentence, such as `1, 2 or 4`.
    [[nodiscard]] static std::string describe(std::uint8_t counts) {
        std::string text;
        for (unsigned count = 1; count <= 128; count *= 2) {
            if ((counts & count) == 0) {
                continue;
            }

            const bool last = (counts & ~((count << 1) - 1)) == 0;
            if (!text.empty()) {
                text += last ? " or " : ", ";
            }

            text += std::to_string(count);
        }

        return text;
    }
};

} // namespace haylen::gpu::direct3d
