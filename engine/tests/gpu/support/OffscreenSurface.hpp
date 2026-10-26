#pragma once

#include <memory>
#include <optional>

#include "haylen/gpu/Device.hpp"
#include "haylen/gpu/Surface.hpp"

namespace haylen::test {

// What a device of the GPU tests draws into without a window: nothing for the APIs that draw offscreen by themselves, and the context of a hidden drawable for the APIs that draw through a `gpu::GlContext`, which the OpenGL device brings to the tests.
class OffscreenSurface final {
  public:
    static constexpr int kWidth = 64;
    static constexpr int kHeight = 64;

    // Throws `std::runtime_error` with the reason when this machine cannot make what the API draws into.
    explicit OffscreenSurface(gpu::Device::Api api);
    OffscreenSurface(const OffscreenSurface&) = delete;
    OffscreenSurface& operator=(const OffscreenSurface&) = delete;
    ~OffscreenSurface();

    [[nodiscard]] const std::optional<gpu::Surface>& getSurface() const noexcept {
        return surface;
    }

  private:
    std::unique_ptr<gpu::GlContext> context;
    std::optional<gpu::Surface> surface;
};

} // namespace haylen::test
