#include "haylen/2d/graphics/Camera.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Geometry.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/Noise2D.hpp"

namespace haylen::graphics2d {

const math::Noise2D& Camera::getShakeNoise() {
    static const math::Noise2D noise(0x5A4B3C2D);
    return noise;
}

float Camera::lerpAngle(float from, float to, float amount) noexcept {
    return from + math::Math::wrapAngle(to - from) * amount;
}

Camera Camera::blend(const Camera& from, const Camera& to, float amount) {
    Camera result = to;
    result.position = math::Vec2::lerp(from.position, to.position, amount);
    result.offset = math::Vec2::lerp(from.offset, to.offset, amount);
    result.rotation = lerpAngle(from.getViewRotation(), to.getViewRotation(), amount);
    result.smoothedRotation = result.rotation;
    result.zoom = {std::exp(math::Math::lerp(std::log(from.zoom.x), std::log(to.zoom.x), amount)), std::exp(math::Math::lerp(std::log(from.zoom.y), std::log(to.zoom.y), amount))};
    if (from.viewport && to.viewport) {
        result.viewport = math::Rect::fromMinMax(math::Vec2::lerp(from.viewport->getMin(), to.viewport->getMin(), amount), math::Vec2::lerp(from.viewport->getMax(), to.viewport->getMax(), amount));
    }
    result.goal = result.position;
    result.following = false;
    return result;
}

void Camera::setZoom(math::Vec2 value) {
    if (std::isnan(value.x) || std::isnan(value.y)) {
        throw std::invalid_argument("The camera zoom must be a number.");
    }
    zoom = {std::clamp(value.x, minZoom, maxZoom), std::clamp(value.y, minZoom, maxZoom)};
}

void Camera::setMinZoom(float value) {
    if (!(value > 0.0F && value <= maxZoom)) {
        throw std::invalid_argument("The smallest zoom must be above 0 and not above the largest zoom.");
    }
    minZoom = value;
    setZoom(zoom);
}

void Camera::setMaxZoom(float value) {
    if (!(value >= minZoom)) {
        throw std::invalid_argument("The largest zoom must not be below the smallest zoom.");
    }
    maxZoom = value;
    setZoom(zoom);
}

math::Vec2 Camera::getWorldView(const math::Rect& screen) const noexcept {
    return getViewRect(screen).getSize() / zoom;
}

math::Insets Camera::getSlack(math::Vec2 worldView) const noexcept {
    const math::Vec2 half = worldView * 0.5F;
    const math::Vec2 dead = deadZone * 0.5F;
    const math::Insets drag = dragMargins;
    return {
        .left = dead.x + (dragHorizontal ? half.x * drag.left : 0.0F),
        .top = dead.y + (dragVertical ? half.y * drag.top : 0.0F),
        .right = dead.x + (dragHorizontal ? half.x * drag.right : 0.0F),
        .bottom = dead.y + (dragVertical ? half.y * drag.bottom : 0.0F),
    };
}

math::Vec2 Camera::getRestingPoint(math::Vec2 target, math::Vec2 worldView) const noexcept {
    const math::Vec2 half = worldView * 0.5F;
    math::Vec2 resting = target;
    if (!dragHorizontal) {
        resting.x += half.x * (dragOffset.x < 0.0F ? dragMargins.right : dragMargins.left) * dragOffset.x;
    }
    if (!dragVertical) {
        resting.y += half.y * (dragOffset.y < 0.0F ? dragMargins.bottom : dragMargins.top) * dragOffset.y;
    }
    return resting;
}

math::Vec2 Camera::clampPoint(math::Vec2 point, math::Vec2 worldView) const noexcept {
    if (!limits) {
        return point;
    }

    // A limit smaller than the view centers the view on it instead.
    const math::Vec2 before = anchor == Anchor::Center ? worldView * 0.5F : math::Vec2{};
    const math::Rect& area = *limits;
    const float x = area.width <= worldView.x ? area.getCenter().x - worldView.x * 0.5F + before.x : std::clamp(point.x, area.getLeft() + before.x, area.getRight() - worldView.x + before.x);
    const float y = area.height <= worldView.y ? area.getCenter().y - worldView.y * 0.5F + before.y : std::clamp(point.y, area.getTop() + before.y, area.getBottom() - worldView.y + before.y);
    return {x, y};
}

float Camera::getViewRotation() const noexcept {
    return rotationSmoothing ? smoothedRotation : rotation;
}

// The goal is where the view heads before smoothing and limits. It stays put while the target moves inside the slack around its resting point.
void Camera::moveGoal(math::Vec2 target, math::Vec2 worldView) noexcept {
    if (anchor == Anchor::TopLeft) {
        goal = target;
        return;
    }
    const math::Vec2 resting = getRestingPoint(target, worldView);
    const math::Insets slack = getSlack(worldView);
    goal.x = std::clamp(goal.x, resting.x - slack.right, resting.x + slack.left);
    goal.y = std::clamp(goal.y, resting.y - slack.bottom, resting.y + slack.top);
}

void Camera::follow(math::Vec2 target, float deltaSeconds, const math::Rect& screen) noexcept {
    const math::Vec2 worldView = getWorldView(screen);
    if (!following) {
        following = true;
        lastTarget = target;
        lead = {};
        goal = position;
    }

    // The look-ahead leads the target by its velocity and eases, so a sudden stop does not jerk the view back.
    if (deltaSeconds > 0.0F) {
        const math::Vec2 wanted = ((target - lastTarget) * (lookAheadTime / deltaSeconds)).clampedLength(maxLookAhead);
        lead += (wanted - lead) * math::Math::dampFactor(lookAheadSmoothingSpeed, deltaSeconds);
    }
    lastTarget = target;
    moveGoal(target + lead, worldView);

    const math::Vec2 wanted = limitSmoothing ? clampPoint(goal, worldView) : goal;
    position = positionSmoothing ? position + (wanted - position) * math::Math::dampFactor(positionSmoothingSpeed, deltaSeconds) : wanted;
    if (!positionSmoothing || !limitSmoothing) {
        position = clampPoint(position, worldView);
    }
}

void Camera::frame(std::span<const math::Vec2> points, float padding, float deltaSeconds, const math::Rect& screen) {
    if (points.empty()) {
        throw std::invalid_argument("Framing needs at least one point.");
    }

    const math::Rect bounds = math::Geometry::bounds(points).expanded(padding);
    const math::Vec2 viewSize = getViewRect(screen).getSize();
    const float fit = std::min(viewSize.x / std::max(bounds.width, 1e-3F), viewSize.y / std::max(bounds.height, 1e-3F));
    const math::Vec2 wanted{fit, fit};
    setZoom(positionSmoothing ? zoom + (wanted - zoom) * math::Math::dampFactor(positionSmoothingSpeed, deltaSeconds) : wanted);

    const math::Vec2 center = bounds.getCenter();
    follow(anchor == Anchor::TopLeft ? center - getWorldView(screen) * 0.5F : center, deltaSeconds, screen);
}

void Camera::update(float deltaSeconds) noexcept {
    smoothedRotation = rotationSmoothing ? lerpAngle(smoothedRotation, rotation, math::Math::dampFactor(rotationSmoothingSpeed, deltaSeconds)) : rotation;

    time += deltaSeconds;
    trauma = std::max(0.0F, trauma - traumaDecay * deltaSeconds);
    if (trauma <= 0.0F) {
        shakeDirection = {};
    }

    // Shake strength grows with the square of trauma so small hits stay subtle.
    const float strength = trauma * trauma;
    const float phase = time * shakeFrequency;
    const math::Noise2D& noise = getShakeNoise();
    if (!shakeDirection.isZero()) {
        shakeOffset = shakeDirection * (noise.simplex(phase, 1.0F) * maxShakeOffset * strength);
        shakeAngle = 0.0F;
        return;
    }
    shakeOffset = math::Vec2{noise.simplex(phase, 1.0F), noise.simplex(phase, 2.0F)} * (maxShakeOffset * strength);
    shakeAngle = noise.simplex(phase, 3.0F) * maxShakeAngle * strength;
}

void Camera::clampToLimits(const math::Rect& screen) noexcept {
    position = clampPoint(position, getWorldView(screen));
}

void Camera::resetSmoothing(const math::Rect& screen) noexcept {
    if (following) {
        position = clampPoint(goal, getWorldView(screen));
    }
    smoothedRotation = rotation;
}

void Camera::align(const math::Rect& screen) noexcept {
    const math::Vec2 worldView = getWorldView(screen);
    if (!following) {
        following = true;
        lastTarget = position;
        lead = {};
    }
    goal = anchor == Anchor::TopLeft ? lastTarget + lead : getRestingPoint(lastTarget + lead, worldView);
}

void Camera::snapTo(math::Vec2 point, const math::Rect& screen) noexcept {
    following = true;
    lastTarget = point;
    lead = {};
    goal = point;
    position = clampPoint(point, getWorldView(screen));
    smoothedRotation = rotation;
}

void Camera::zoomAt(float factor, math::Vec2 screenPoint, const math::Rect& screen) {
    const math::Vec2 before = screenToWorld(screenPoint, screen);
    setZoom(zoom * factor);
    const math::Vec2 moved = before - screenToWorld(screenPoint, screen);
    position += moved;
    goal += moved;
}

void Camera::addTrauma(float amount) noexcept {
    trauma = math::Math::saturate(trauma + amount);
    shakeDirection = {};
}

void Camera::shake(float amount, math::Vec2 direction) noexcept {
    trauma = math::Math::saturate(trauma + amount);
    shakeDirection = direction.getNormalized();
}

void Camera::setTrauma(float value) noexcept {
    trauma = math::Math::saturate(value);
}

math::Vec2 Camera::getRenderPosition() const noexcept {
    const math::Vec2 shaken = position + offset + shakeOffset;
    return pixelSnap ? math::Vec2::round(shaken * zoom) / zoom : shaken;
}

float Camera::getRenderRotation() const noexcept {
    return ignoreRotation ? 0.0F : getViewRotation() + shakeAngle;
}

math::Transform2D Camera::viewTransform(math::Vec2 viewSize) const noexcept {
    const math::Vec2 pivot = anchor == Anchor::Center ? viewSize * 0.5F : math::Vec2{};
    return math::Transform2D::translation(pivot) * math::Transform2D::scaling(zoom) * math::Transform2D::rotation(-getRenderRotation()) * math::Transform2D::translation(-getRenderPosition());
}

math::Vec2 Camera::worldToScreen(math::Vec2 world, const math::Rect& screen) const noexcept {
    const math::Rect view = getViewRect(screen);
    return view.getMin() + viewTransform(view.getSize()).apply(world);
}

math::Vec2 Camera::screenToWorld(math::Vec2 point, const math::Rect& screen) const noexcept {
    const math::Rect view = getViewRect(screen);
    return viewTransform(view.getSize()).getInverse().apply(point - view.getMin());
}

math::Rect Camera::visibleBounds(const math::Rect& screen) const noexcept {
    const math::Rect view = getViewRect(screen);
    const std::array<math::Vec2, 4> corners{screenToWorld(view.getMin(), screen), screenToWorld({view.getRight(), view.getTop()}, screen), screenToWorld(view.getMax(), screen), screenToWorld({view.getLeft(), view.getBottom()}, screen)};
    return math::Geometry::bounds(corners);
}

void Camera::drawDebug(Renderer& renderer, const math::Rect& screen, const DrawOrder& order) const {
    const float thickness = kDebugThickness * renderer.getCanvasUnitSize();
    const math::Rect view = getViewRect(screen);
    const std::array<math::Vec2, 4> corners{screenToWorld(view.getMin(), screen), screenToWorld({view.getRight(), view.getTop()}, screen), screenToWorld(view.getMax(), screen), screenToWorld({view.getLeft(), view.getBottom()}, screen)};
    renderer.drawPolyline(corners, thickness, math::Color::fromHex(0xFFFFFFC0U), true, order);
    if (limits) {
        renderer.drawRectOutline(*limits, thickness, math::Color::fromHex(0xFF5A4AE0U), order);
    }
    if (anchor == Anchor::TopLeft) {
        return;
    }

    // The target moves inside this box without moving the goal, which the view follows.
    const math::Vec2 worldView = getWorldView(screen);
    const math::Insets slack = getSlack(worldView);
    const math::Vec2 rest = getRestingPoint({}, worldView);
    const math::Vec2 anchorPoint = following ? goal : position;
    const math::Rect box = math::Rect::fromMinMax(anchorPoint - math::Vec2{slack.left, slack.top} - rest, anchorPoint + math::Vec2{slack.right, slack.bottom} - rest);
    renderer.drawRectOutline(box, thickness, math::Color::fromHex(0xFFD84AE0U), order);
    if (following) {
        const float arm = thickness * 6.0F;
        renderer.drawLine(lastTarget - math::Vec2{arm, 0.0F}, lastTarget + math::Vec2{arm, 0.0F}, thickness, math::Color::fromHex(0x4AD8FFFFU), order);
        renderer.drawLine(lastTarget - math::Vec2{0.0F, arm}, lastTarget + math::Vec2{0.0F, arm}, thickness, math::Color::fromHex(0x4AD8FFFFU), order);
    }
}

} // namespace haylen::graphics2d
