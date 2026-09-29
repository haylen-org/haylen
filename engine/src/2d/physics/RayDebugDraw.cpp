#include "haylen/2d/physics/RayDebugDraw.hpp"

#include "haylen/2d/graphics/Renderer.hpp"

namespace haylen::physics2d {

const math::Color RayDebugDraw::kMissColor = math::Color::fromHex(0x66CC66CCU);
const math::Color RayDebugDraw::kHitColor = math::Color::fromHex(0xFF5544EEU);
const math::Color RayDebugDraw::kNormalColor = math::Color::fromHex(0x55AAFFEEU);

void RayDebugDraw::add(math::Vec2 from, math::Vec2 to, const std::optional<Hit>& hit) {
    rays.push_back({.from = from, .to = to, .hit = hit});
}

void RayDebugDraw::drawRay(graphics2d::Renderer& renderer, math::Vec2 from, math::Vec2 to, const std::optional<Hit>& hit, const graphics2d::DrawOrder& order) {
    if (!hit) {
        renderer.drawLine(from, to, kLineWidth, kMissColor, order);
        return;
    }
    renderer.drawLine(from, hit->point, kLineWidth, kHitColor, order);
    renderer.drawCircle(hit->point, kHitRadius, kHitColor, order);
    if (!hit->normal.isZero()) {
        renderer.drawLine(hit->point, hit->point + hit->normal * kNormalLength, kLineWidth, kNormalColor, order);
    }
}

void RayDebugDraw::draw(graphics2d::Renderer& renderer, const graphics2d::DrawOrder& order) {
    for (const Ray& ray : rays) {
        drawRay(renderer, ray.from, ray.to, ray.hit, order);
    }
    rays.clear();
}

} // namespace haylen::physics2d
