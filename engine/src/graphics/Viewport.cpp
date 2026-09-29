#include "haylen/graphics/Viewport.hpp"

#include <algorithm>
#include <cmath>

namespace haylen::graphics {

const std::array<std::pair<std::string_view, Viewport::ScalingPolicy>, 5> Viewport::kPolicyNames = {{
    {"fit", ScalingPolicy::Fit},
    {"fill", ScalingPolicy::Fill},
    {"stretch", ScalingPolicy::Stretch},
    {"expand", ScalingPolicy::Expand},
    {"pixel_perfect", ScalingPolicy::PixelPerfect},
}};

std::optional<Viewport::ScalingPolicy> Viewport::scalingPolicyFromName(std::string_view name) noexcept {
    for (const auto& [candidate, value] : kPolicyNames) {
        if (candidate == name) {
            return value;
        }
    }
    return std::nullopt;
}

math::Rect Viewport::centered(math::Vec2 container, math::Vec2 size) noexcept {
    return {(container.x - size.x) * 0.5F, (container.y - size.y) * 0.5F, size.x, size.y};
}

void Viewport::update(math::Vec2 framebuffer, math::Vec2 design, ScalingPolicy scaling, const math::Insets& safeInsetsPixels) noexcept {
    framebufferSize = {std::max(1.0F, framebuffer.x), std::max(1.0F, framebuffer.y)};
    designSize = {std::max(1.0F, design.x), std::max(1.0F, design.y)};
    policy = scaling;

    const math::Vec2 ratio = framebufferSize / designSize;
    const math::Rect designRect{0.0F, 0.0F, designSize.x, designSize.y};
    const math::Rect fullFramebuffer{0.0F, 0.0F, framebufferSize.x, framebufferSize.y};

    switch (policy) {
    case ScalingPolicy::Fit: {
        const float scale = std::min(ratio.x, ratio.y);
        pixelRect = centered(framebufferSize, designSize * scale);
        visibleRect = designRect;
        break;
    }
    case ScalingPolicy::PixelPerfect: {
        // A framebuffer smaller than the design area shrinks it by a whole divisor, so the whole design stays visible at an integer ratio of pixels.
        const float fit = std::min(ratio.x, ratio.y);
        const math::Vec2 scaled = fit >= 1.0F ? designSize * std::floor(fit) : designSize / std::ceil(std::max(designSize.x / framebufferSize.x, designSize.y / framebufferSize.y));
        pixelRect = centered(framebufferSize, scaled);
        visibleRect = designRect;
        break;
    }
    case ScalingPolicy::Fill: {
        const float scale = std::max(ratio.x, ratio.y);
        pixelRect = fullFramebuffer;
        visibleRect = centered(designSize, framebufferSize / scale);
        break;
    }
    case ScalingPolicy::Stretch:
        pixelRect = fullFramebuffer;
        visibleRect = designRect;
        break;
    case ScalingPolicy::Expand: {
        const float scale = std::min(ratio.x, ratio.y);
        pixelRect = fullFramebuffer;
        visibleRect = centered(designSize, framebufferSize / scale);
        break;
    }
    }

    const math::Rect safePixels = fullFramebuffer.inset(safeInsetsPixels).intersection(pixelRect);
    safeRect = math::Rect::fromMinMax(toDesign(safePixels.getMin()), toDesign(safePixels.getMax()));
}

math::Vec2 Viewport::getPixelsPerUnit() const noexcept {
    return pixelRect.getSize() / visibleRect.getSize();
}

math::Vec2 Viewport::toDesign(math::Vec2 framebufferPoint) const noexcept {
    return visibleRect.getMin() + (framebufferPoint - pixelRect.getMin()) / getPixelsPerUnit();
}

math::Vec2 Viewport::toFramebuffer(math::Vec2 designPoint) const noexcept {
    return pixelRect.getMin() + (designPoint - visibleRect.getMin()) * getPixelsPerUnit();
}

} // namespace haylen::graphics
