#pragma once

#include <box2d/box2d.h>

#include "haylen/2d/graphics/DrawOrder.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics2d {
class Renderer;
}

namespace haylen::physics2d {

// Draws the shapes and joints of a Box2D world with the 2D renderer, in world units.
class DebugDraw final {
  public:
    DebugDraw(graphics2d::Renderer& target, const graphics2d::DrawOrder& drawOrder, float pixelsPerMeter) noexcept : renderer(target), order(drawOrder), scale(pixelsPerMeter) {}

    void draw(b2WorldId world);

  private:
    static constexpr float kLineWidth = 2.0F;

    [[nodiscard]] static math::Color toColor(b2HexColor color) noexcept;
    [[nodiscard]] math::Vec2 toPixels(b2Vec2 value) const noexcept;

    static void drawPolygon(const b2Vec2* vertices, int count, b2HexColor color, void* context);
    static void drawSolidPolygon(b2Transform transform, const b2Vec2* vertices, int count, float radius, b2HexColor color, void* context);
    static void drawCircle(b2Vec2 center, float radius, b2HexColor color, void* context);
    static void drawSolidCircle(b2Transform transform, float radius, b2HexColor color, void* context);
    static void drawSolidCapsule(b2Vec2 first, b2Vec2 second, float radius, b2HexColor color, void* context);
    static void drawSegment(b2Vec2 first, b2Vec2 second, b2HexColor color, void* context);
    static void drawPoint(b2Vec2 point, float size, b2HexColor color, void* context);

    graphics2d::Renderer& renderer;
    graphics2d::DrawOrder order;
    float scale;
};

} // namespace haylen::physics2d
