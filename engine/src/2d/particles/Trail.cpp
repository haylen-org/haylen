#include "haylen/2d/particles/Trail.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/particles/Ribbon.hpp"

namespace haylen::particles2d {

void Trail::validate(const Options& settings) {
    if (!(settings.lifetime > 0.0F && std::isfinite(settings.lifetime)) || !(settings.minDistance >= 0.0F && std::isfinite(settings.minDistance))) {
        throw std::invalid_argument("A trail needs a positive, finite lifetime and a minimum distance of at least 0.");
    }
    if (settings.maxPoints < 2 || settings.maxPoints > kMaxPoints) {
        throw std::invalid_argument("A trail keeps from 2 to 4096 points.");
    }
    if (!(settings.widthStart >= 0.0F && settings.widthEnd >= 0.0F) || settings.colors.empty()) {
        throw std::invalid_argument("A trail needs widths of at least 0 and at least one color.");
    }
}

Trail::Trail(Options settings) : options(std::move(settings)) {
    validate(options);
    points.reserve(options.maxPoints);
}

void Trail::setOptions(Options value) {
    validate(value);
    options = std::move(value);
    if (points.size() > options.maxPoints) {
        points.erase(points.begin(), points.end() - static_cast<std::ptrdiff_t>(options.maxPoints));
    }
}

void Trail::update(float deltaSeconds) {
    for (Point& point : points) {
        point.age += deltaSeconds;
    }
    const auto expired = std::find_if(points.begin(), points.end(), [this](const Point& point) { return point.age < options.lifetime; });
    points.erase(points.begin(), expired);

    if (emitting && (points.empty() || math::Vec2::distance(points.back().position, position) >= options.minDistance)) {
        if (points.size() == options.maxPoints) {
            points.erase(points.begin());
        }
        points.push_back({.position = position});
    }
}

// The head follows the position while the trail emits, so the ribbon reaches the moving point between the points it keeps.
void Trail::draw(graphics2d::Renderer& renderer) const {
    line.clear();
    places.clear();
    if (emitting) {
        line.push_back(position);
        places.push_back(0.0F);
    }
    for (auto point = points.rbegin(); point != points.rend(); ++point) {
        line.push_back(point->position);
        places.push_back(std::min(point->age / options.lifetime, 1.0F));
    }
    if (line.size() < 2) {
        return;
    }

    vertices.clear();
    indices.clear();
    Ribbon::append(line, places, {.widthStart = options.widthStart, .widthEnd = options.widthEnd, .colors = options.colors}, vertices, indices);
    renderer.drawMesh(options.texture, vertices, indices, options.order);
}

void Trail::clear() noexcept {
    points.clear();
}

} // namespace haylen::particles2d
