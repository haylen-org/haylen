#include "haylen/2d/lighting/Occluder.hpp"

#include <box2d/box2d.h>

#include <algorithm>
#include <stdexcept>

#include "2d/physics/Box2DConverter.hpp"
#include "haylen/2d/physics/Body.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/2d/tiled/MapQuery.hpp"
#include "haylen/2d/tiled/MapRenderer.hpp"
#include "haylen/math/Math.hpp"

namespace haylen::lighting2d {

const std::array<std::string_view, 3> Occluder::kCullNames = {"disabled", "clockwise", "counterClockwise"};

std::optional<Occluder::Cull> Occluder::cullFromName(std::string_view name) noexcept {
    const auto found = std::find(kCullNames.begin(), kCullNames.end(), name);
    return found == kCullNames.end() ? std::nullopt : std::optional(static_cast<Cull>(found - kCullNames.begin()));
}

std::string_view Occluder::cullName(Cull value) noexcept {
    return kCullNames[static_cast<std::size_t>(value)];
}

std::vector<Occluder> Occluder::fromBody(const physics2d::World& world, const physics2d::Body& body) {
    const float scale = world.getPixelsPerMeter();
    const auto pixels = [scale](b2Vec2 value) { return physics2d::Box2DConverter::toPixels(value, scale); };
    const Occluder placed{.position = body.getPosition(), .rotation = body.getRotation()};

    std::vector<Occluder> occluders;
    for (const physics2d::Shape& shape : body.getShapes()) {
        const b2ShapeId id = b2LoadShapeId(shape.getId());
        Occluder occluder = placed;
        switch (b2Shape_GetType(id)) {
        case b2_polygonShape: {
            const b2Polygon polygon = b2Shape_GetPolygon(id);
            for (int index = 0; index < polygon.count; ++index) {
                occluder.points.push_back(pixels(polygon.vertices[index]));
            }
            break;
        }
        case b2_circleShape: {
            const b2Circle circle = b2Shape_GetCircle(id);
            for (int index = 0; index < kCurveSegments; ++index) {
                occluder.points.push_back(pixels(circle.center) + math::Vec2::fromAngle(math::Math::kTau * static_cast<float>(index) / static_cast<float>(kCurveSegments), circle.radius * scale));
            }
            break;
        }
        case b2_capsuleShape: {
            // Each cap is half a circle around its center, turned away from the other center.
            const b2Capsule capsule = b2Shape_GetCapsule(id);
            const math::Vec2 first = pixels(capsule.center1);
            const math::Vec2 second = pixels(capsule.center2);
            const float facing = (second - first).getAngle();
            const int half = kCurveSegments / 2;
            for (int index = 0; index <= half; ++index) {
                occluder.points.push_back(second + math::Vec2::fromAngle(facing - math::Math::kHalfPi + math::Math::kPi * static_cast<float>(index) / static_cast<float>(half), capsule.radius * scale));
            }
            for (int index = 0; index <= half; ++index) {
                occluder.points.push_back(first + math::Vec2::fromAngle(facing + math::Math::kHalfPi + math::Math::kPi * static_cast<float>(index) / static_cast<float>(half), capsule.radius * scale));
            }
            break;
        }
        case b2_segmentShape: {
            const b2Segment segment = b2Shape_GetSegment(id);
            occluder.points = {pixels(segment.point1), pixels(segment.point2)};
            occluder.closed = false;
            break;
        }
        case b2_chainSegmentShape: {
            // The segments of a chain come one after another, so each one extends the chain the previous one started.
            const b2Segment segment = b2Shape_GetChainSegment(id).segment;
            const math::Vec2 from = pixels(segment.point1);
            const math::Vec2 to = pixels(segment.point2);
            if (!occluders.empty() && !occluders.back().closed && occluders.back().points.back() == from) {
                occluders.back().points.push_back(to);
                continue;
            }
            if (!occluders.empty() && !occluders.back().closed && occluders.back().points.front() == to) {
                occluders.back().points.insert(occluders.back().points.begin(), from);
                continue;
            }
            occluder.points = {from, to};
            occluder.closed = false;
            break;
        }
        default:
            continue;
        }
        occluders.push_back(std::move(occluder));
    }

    // A chain that returns to its start is a loop.
    for (Occluder& occluder : occluders) {
        if (!occluder.closed && occluder.points.size() > 3 && occluder.points.front() == occluder.points.back()) {
            occluder.points.pop_back();
            occluder.closed = true;
        }
    }
    return occluders;
}

std::vector<Occluder> Occluder::fromMap(const tiled::MapRenderer& map, std::string_view layer) {
    const tiled::MapQuery query(map.getMap());
    std::vector<Occluder> occluders;
    std::vector<math::Vec2> points;
    // clang-format off
    map.forEachObject(layer, [&](const tiled::Object& object, math::Vec2 position) {
        query.getOutline(object, position - map.getMap().objectToWorld(object.position), points);
        const bool closed = object.shape != tiled::Object::Shape::Polyline;
        if (points.size() >= (closed ? 3U : 2U)) {
            occluders.push_back({.points = points, .closed = closed});
        }
    });
    // clang-format on
    return occluders;
}

void Occluder::validate() const {
    if (points.size() < (closed ? 3U : 2U)) {
        throw std::invalid_argument("An occluder needs at least 2 points, and 3 when it is closed.");
    }
}

std::vector<math::Vec2> Occluder::getWorldPoints() const {
    std::vector<math::Vec2> placed;
    placed.reserve(points.size());
    for (const math::Vec2 point : points) {
        placed.push_back(position + (point * scale).rotated(rotation));
    }
    return placed;
}

} // namespace haylen::lighting2d
