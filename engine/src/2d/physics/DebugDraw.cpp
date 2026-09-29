#include "2d/physics/DebugDraw.hpp"

#include <cstdint>
#include <vector>

#include "haylen/2d/graphics/Renderer.hpp"

namespace haylen::physics2d {

void DebugDraw::draw(b2WorldId world) {
    b2DebugDraw callbacks = b2DefaultDebugDraw();
    callbacks.context = this;
    callbacks.drawShapes = true;
    callbacks.drawJoints = true;
    callbacks.DrawPolygonFcn = &drawPolygon;
    callbacks.DrawSolidPolygonFcn = &drawSolidPolygon;
    callbacks.DrawCircleFcn = &drawCircle;
    callbacks.DrawSolidCircleFcn = &drawSolidCircle;
    callbacks.DrawSolidCapsuleFcn = &drawSolidCapsule;
    callbacks.DrawSegmentFcn = &drawSegment;
    callbacks.DrawTransformFcn = &drawTransform;
    callbacks.DrawPointFcn = &drawPoint;
    callbacks.DrawStringFcn = &drawString;
    b2World_Draw(world, &callbacks);
}

math::Color DebugDraw::toColor(b2HexColor color) noexcept {
    const auto value = static_cast<std::uint32_t>(color);
    return math::Color::fromHex((value << 8U) | 0xCCU);
}

math::Vec2 DebugDraw::toPixels(b2Vec2 value) const noexcept {
    return {value.x * scale, value.y * scale};
}

void DebugDraw::drawPolygon(const b2Vec2* vertices, int count, b2HexColor color, void* context) {
    const auto& self = *static_cast<DebugDraw*>(context);
    std::vector<math::Vec2> points;
    for (int index = 0; index < count; ++index) {
        points.push_back(self.toPixels(vertices[index]));
    }
    self.renderer.drawPolyline(points, kLineWidth, toColor(color), true, self.order);
}

void DebugDraw::drawSolidPolygon(b2Transform transform, const b2Vec2* vertices, int count, float, b2HexColor color, void* context) {
    const auto& self = *static_cast<DebugDraw*>(context);
    std::vector<math::Vec2> points;
    for (int index = 0; index < count; ++index) {
        points.push_back(self.toPixels(b2TransformPoint(transform, vertices[index])));
    }
    self.renderer.drawPolyline(points, kLineWidth, toColor(color), true, self.order);
}

void DebugDraw::drawCircle(b2Vec2 center, float radius, b2HexColor color, void* context) {
    const auto& self = *static_cast<DebugDraw*>(context);
    self.renderer.drawRing(self.toPixels(center), radius * self.scale, kLineWidth, toColor(color), self.order);
}

void DebugDraw::drawSolidCircle(b2Transform transform, float radius, b2HexColor color, void* context) {
    const auto& self = *static_cast<DebugDraw*>(context);
    self.renderer.drawRing(self.toPixels(transform.p), radius * self.scale, kLineWidth, toColor(color), self.order);
}

void DebugDraw::drawSolidCapsule(b2Vec2 first, b2Vec2 second, float radius, b2HexColor color, void* context) {
    const auto& self = *static_cast<DebugDraw*>(context);
    self.renderer.drawRing(self.toPixels(first), radius * self.scale, kLineWidth, toColor(color), self.order);
    self.renderer.drawRing(self.toPixels(second), radius * self.scale, kLineWidth, toColor(color), self.order);
    self.renderer.drawLine(self.toPixels(first), self.toPixels(second), kLineWidth, toColor(color), self.order);
}

void DebugDraw::drawSegment(b2Vec2 first, b2Vec2 second, b2HexColor color, void* context) {
    const auto& self = *static_cast<DebugDraw*>(context);
    self.renderer.drawLine(self.toPixels(first), self.toPixels(second), kLineWidth, toColor(color), self.order);
}

void DebugDraw::drawTransform(b2Transform, void*) {}

void DebugDraw::drawPoint(b2Vec2 point, float size, b2HexColor color, void* context) {
    const auto& self = *static_cast<DebugDraw*>(context);
    self.renderer.drawCircle(self.toPixels(point), size * 0.5F, toColor(color), self.order);
}

void DebugDraw::drawString(b2Vec2, const char*, b2HexColor, void*) {}

} // namespace haylen::physics2d
