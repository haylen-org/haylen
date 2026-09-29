#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/math/Insets.hpp"
#include "haylen/math/Rect.hpp"

namespace haylen::graphics {

// Maps the fixed design space onto the framebuffer.
class Viewport final {
  public:
    enum class ScalingPolicy : std::uint8_t {
        // Shows the whole design area with letterbox bars.
        Fit,
        // Fills the screen and crops the design area.
        Fill,
        // Fills the screen with non-uniform scaling.
        Stretch,
        // Keeps the design area whole and centered, and extends the visible area to fill the screen.
        Expand,
        // Like Fit, but only with integer scale factors, or integer divisors on framebuffers smaller than the design area.
        PixelPerfect,
    };

    // Resolves the names "fit", "fill", "stretch", "expand" and "pixelPerfect".
    [[nodiscard]] static std::optional<ScalingPolicy> scalingPolicyFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view scalingPolicyName(ScalingPolicy value) noexcept;

    void update(math::Vec2 framebuffer, math::Vec2 design, ScalingPolicy scaling, const math::Insets& safeInsetsPixels = {}) noexcept;

    [[nodiscard]] math::Vec2 getFramebufferSize() const noexcept {
        return framebufferSize;
    }
    [[nodiscard]] math::Vec2 getDesignSize() const noexcept {
        return designSize;
    }
    [[nodiscard]] ScalingPolicy getScaling() const noexcept {
        return policy;
    }

    // Framebuffer region, in pixels, that receives the visible design rectangle.
    [[nodiscard]] const math::Rect& getPixelRect() const noexcept {
        return pixelRect;
    }

    // Region of design space that is visible on screen. It can exceed the design size with Expand.
    [[nodiscard]] const math::Rect& getVisibleRect() const noexcept {
        return visibleRect;
    }

    // Visible design region that is not covered by notches, rounded corners or system bars.
    [[nodiscard]] const math::Rect& getSafeRect() const noexcept {
        return safeRect;
    }

    [[nodiscard]] math::Vec2 getPixelsPerUnit() const noexcept;
    [[nodiscard]] math::Vec2 toDesign(math::Vec2 framebufferPoint) const noexcept;
    [[nodiscard]] math::Vec2 toFramebuffer(math::Vec2 designPoint) const noexcept;

  private:
    static const std::array<std::pair<std::string_view, ScalingPolicy>, 5> kPolicyNames;

    [[nodiscard]] static math::Rect centered(math::Vec2 container, math::Vec2 size) noexcept;

    math::Vec2 framebufferSize{1920.0F, 1080.0F};
    math::Vec2 designSize{1920.0F, 1080.0F};
    ScalingPolicy policy = ScalingPolicy::Fit;
    math::Rect pixelRect{0.0F, 0.0F, 1920.0F, 1080.0F};
    math::Rect visibleRect{0.0F, 0.0F, 1920.0F, 1080.0F};
    math::Rect safeRect{0.0F, 0.0F, 1920.0F, 1080.0F};
};

} // namespace haylen::graphics
