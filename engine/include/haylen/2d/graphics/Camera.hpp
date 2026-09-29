#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "haylen/2d/graphics/DrawOrder.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Transform2D.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::math {
class Noise2D;
}

namespace haylen::graphics2d {

class Renderer;

// 2D camera whose position is the point of the world it shows. The view covers the camera viewport, or the whole screen without one, divided by the zoom. The screen is the area the camera draws into: the visible design area for world canvases and the whole target for render target canvases, and the viewport is a rectangle in the same coordinates. Following moves the position toward a target through the dead zone, the drag margins, the look-ahead, the smoothing and the limits, and updating advances the shake and the rotation smoothing.
class Camera final {
  public:
    enum class Anchor : std::uint8_t {
        // The position is the center of the view, which rotates and zooms around it.
        Center,
        // The position is the top-left corner of the view, and following ignores the dead zone and the drag margins.
        TopLeft,
    };

    math::Vec2 position{};

    // Moves the view in world units after the limits, so it can show past them.
    math::Vec2 offset{};
    Anchor anchor = Anchor::Center;
    float rotation = 0.0F;
    bool ignoreRotation = false;
    std::optional<math::Rect> viewport;
    std::optional<math::Rect> limits;

    // Eases the view into the limits instead of stopping it at once, when position smoothing is on.
    bool limitSmoothing = false;
    bool positionSmoothing = false;
    float positionSmoothingSpeed = 5.0F;
    bool rotationSmoothing = false;
    float rotationSmoothingSpeed = 5.0F;

    // Size in world units of the box around the view center where the target moves without moving the view.
    math::Vec2 deadZone{};
    bool dragHorizontal = false;
    bool dragVertical = false;

    // How far the target moves away from the view center on each side before the view follows, in fractions of half the view.
    math::Insets dragMargins = math::Insets::uniform(0.2F);

    // Where the target rests on an axis that does not drag, from -1 on the right or bottom margin to 1 on the left or top margin.
    math::Vec2 dragOffset{};

    // Seconds of target velocity the view leads by, up to the largest distance in world units, eased at the speed.
    float lookAheadTime = 0.0F;
    float maxLookAhead = 200.0F;
    float lookAheadSmoothingSpeed = 4.0F;
    float maxShakeOffset = 24.0F;
    float maxShakeAngle = 0.05F;
    float shakeFrequency = 25.0F;
    float traumaDecay = 1.5F;
    bool pixelSnap = false;

    // Returns a camera between the two views, where 0 is the first camera and 1 the second. It takes the settings of the second camera, and the zoom changes at a steady rate.
    [[nodiscard]] static Camera blend(const Camera& from, const Camera& to, float amount);

    [[nodiscard]] math::Vec2 getZoom() const noexcept {
        return zoom;
    }

    // Keeps each axis of the zoom between the zoom limits.
    void setZoom(math::Vec2 value) noexcept;
    [[nodiscard]] float getMinZoom() const noexcept {
        return minZoom;
    }
    [[nodiscard]] float getMaxZoom() const noexcept {
        return maxZoom;
    }

    // The smallest zoom is above 0 and never above the largest one. Changing a limit clamps the zoom again.
    void setMinZoom(float value);
    void setMaxZoom(float value);

    // Moves the position toward the target with the dead zone, the drag margins, the look-ahead, the smoothing and the limits.
    void follow(math::Vec2 target, float deltaSeconds, const math::Rect& screen) noexcept;

    // Follows the middle of the points and zooms until they fit inside the view with the padding in world units, within the zoom limits. The zoom eases like the position when position smoothing is on.
    void frame(std::span<const math::Vec2> points, float padding, float deltaSeconds, const math::Rect& screen);

    // Advances the shake and the rotation smoothing.
    void update(float deltaSeconds) noexcept;

    void clampToLimits(const math::Rect& screen) noexcept;

    // Puts the view where following wants it now, without easing the position or the rotation.
    void resetSmoothing(const math::Rect& screen) noexcept;

    // Recenters following on the last target, as if the target had just been reached, and lets the smoothing ease the view there.
    void align(const math::Rect& screen) noexcept;

    // Moves the view to the point at once.
    void snapTo(math::Vec2 point, const math::Rect& screen) noexcept;

    // Multiplies the zoom by the factor and keeps the world point under the screen point in place, for pinch and wheel zoom.
    void zoomAt(float factor, math::Vec2 screenPoint, const math::Rect& screen) noexcept;

    void addTrauma(float amount) noexcept;

    // Adds trauma that shakes the view only back and forth along the direction until it settles.
    void shake(float amount, math::Vec2 direction) noexcept;

    [[nodiscard]] float getTrauma() const noexcept {
        return trauma;
    }
    void setTrauma(float value) noexcept;
    [[nodiscard]] math::Vec2 getShakeOffset() const noexcept {
        return shakeOffset;
    }
    [[nodiscard]] math::Vec2 getRenderPosition() const noexcept;
    [[nodiscard]] float getRenderRotation() const noexcept;
    [[nodiscard]] math::Rect getViewRect(const math::Rect& screen) const noexcept {
        return viewport.value_or(screen);
    }

    // Maps world coordinates to view coordinates, where (0, 0) is the top-left of the view and the view has the given size.
    [[nodiscard]] math::Transform2D viewTransform(math::Vec2 viewSize) const noexcept;
    [[nodiscard]] math::Vec2 worldToScreen(math::Vec2 world, const math::Rect& screen) const noexcept;
    [[nodiscard]] math::Vec2 screenToWorld(math::Vec2 point, const math::Rect& screen) const noexcept;
    [[nodiscard]] math::Rect visibleBounds(const math::Rect& screen) const noexcept;

    // Draws the view, the limits and the area where the target moves without moving the view, in world coordinates with lines two units of the destination thick, so a world canvas of any camera shows them.
    void drawDebug(Renderer& renderer, const math::Rect& screen, const DrawOrder& order = {}) const;

  private:
    static constexpr float kDebugThickness = 2.0F;

    [[nodiscard]] static const math::Noise2D& getShakeNoise();
    [[nodiscard]] static float lerpAngle(float from, float to, float amount) noexcept;

    // Returns the view size in world units.
    [[nodiscard]] math::Vec2 getWorldView(const math::Rect& screen) const noexcept;

    // Returns how far in world units the goal may trail the target on each side before it moves.
    [[nodiscard]] math::Insets getSlack(math::Vec2 worldView) const noexcept;

    // Returns where the goal rests for a target on the axes that do not drag.
    [[nodiscard]] math::Vec2 getRestingPoint(math::Vec2 target, math::Vec2 worldView) const noexcept;
    [[nodiscard]] math::Vec2 clampPoint(math::Vec2 point, math::Vec2 worldView) const noexcept;
    [[nodiscard]] float getViewRotation() const noexcept;
    void moveGoal(math::Vec2 target, math::Vec2 worldView) noexcept;

    math::Vec2 zoom{1.0F, 1.0F};
    float minZoom = 0.05F;
    float maxZoom = 20.0F;
    math::Vec2 goal{};
    math::Vec2 lastTarget{};
    math::Vec2 lead{};
    bool following = false;
    float smoothedRotation = 0.0F;
    float trauma = 0.0F;
    math::Vec2 shakeDirection{};
    float time = 0.0F;
    math::Vec2 shakeOffset{};
    float shakeAngle = 0.0F;
};

} // namespace haylen::graphics2d
