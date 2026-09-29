#include "haylen/2d/tiled/MapQuery.hpp"

#include <cmath>
#include <numbers>
#include <stdexcept>
#include <string>

#include "haylen/2d/spatial/GridRay.hpp"
#include "haylen/math/RayHit.hpp"
#include "haylen/math/Raycast.hpp"

namespace haylen::tiled {

math::Vec2 MapQuery::rotateAround(math::Vec2 point, math::Vec2 origin, float angle) noexcept {
    return (point - origin).rotated(angle) + origin;
}

std::optional<std::pair<const Layer*, math::Vec2>> MapQuery::findLayer(std::string_view name) const {
    std::optional<std::pair<const Layer*, math::Vec2>> found;
    // clang-format off
    const auto search = [&](const auto& self, const std::vector<Layer>& layers, math::Vec2 offset) -> void {
        for (const Layer& layer : layers) {
            if (found) {
                return;
            }
            if (layer.name == name) {
                found = std::pair{&layer, offset + layer.offset};
                return;
            }
            self(self, layer.layers, offset + layer.offset);
        }
    };
    // clang-format on
    search(search, map.layers, {});
    return found;
}

template <typename Visit> void MapQuery::forEachObject(std::string_view layer, Visit&& visit) const {
    if (!layer.empty()) {
        const std::optional<std::pair<const Layer*, math::Vec2>> found = findLayer(layer);
        if (!found || found->first->kind != Layer::Kind::Object) {
            throw std::invalid_argument("The map has no object layer named '" + std::string(layer) + "'.");
        }
        for (const Object& object : found->first->objects) {
            visit(object, found->second);
        }
        return;
    }

    // clang-format off
    const auto walk = [&](const auto& self, const std::vector<Layer>& layers, math::Vec2 offset) -> void {
        for (const Layer& child : layers) {
            for (const Object& object : child.objects) {
                visit(object, offset + child.offset);
            }
            self(self, child.layers, offset + child.offset);
        }
    };
    // clang-format on
    walk(walk, map.layers, {});
}

void MapQuery::getOutline(const Object& object, math::Vec2 offset, std::vector<math::Vec2>& points) const {
    points.clear();
    const math::Vec2 origin = object.position;
    const auto place = [&](math::Vec2 point) { points.push_back(map.objectToWorld(rotateAround(point, origin, object.rotation)) + offset); };
    const float tau = 2.0F * std::numbers::pi_v<float>;

    switch (object.shape) {
    case Object::Shape::Point:
    case Object::Shape::Text:
        return;
    case Object::Shape::Rectangle:
    case Object::Shape::Tile: {
        // Tile objects sit on their bottom-left corner while rectangles hang from their top-left corner.
        const math::Vec2 top = object.shape == Object::Shape::Tile ? origin - math::Vec2{0.0F, object.size.y} : origin;
        for (const math::Vec2 corner : {top, top + math::Vec2{object.size.x, 0.0F}, top + object.size, top + math::Vec2{0.0F, object.size.y}}) {
            place(corner);
        }
        return;
    }
    case Object::Shape::Ellipse: {
        const math::Vec2 radii = object.size * 0.5F;
        for (int segment = 0; segment < kCurveSegments; ++segment) {
            const float angle = tau * static_cast<float>(segment) / static_cast<float>(kCurveSegments);
            place(origin + radii + math::Vec2{std::cos(angle) * radii.x, std::sin(angle) * radii.y});
        }
        return;
    }
    case Object::Shape::Capsule: {
        // Round caps close the two short sides, around centers one radius in from them.
        const bool wide = object.size.x >= object.size.y;
        const float radius = (wide ? object.size.y : object.size.x) * 0.5F;
        const math::Vec2 first = origin + math::Vec2{radius, radius};
        const math::Vec2 second = wide ? origin + math::Vec2{object.size.x - radius, radius} : origin + math::Vec2{radius, object.size.y - radius};
        const float start = wide ? -std::numbers::pi_v<float> * 0.5F : 0.0F;
        const int half = kCurveSegments / 2;
        for (int segment = 0; segment <= half; ++segment) {
            place(second + math::Vec2::fromAngle(start + std::numbers::pi_v<float> * static_cast<float>(segment) / static_cast<float>(half), radius));
        }
        for (int segment = 0; segment <= half; ++segment) {
            place(first + math::Vec2::fromAngle(start + std::numbers::pi_v<float> * (1.0F + static_cast<float>(segment) / static_cast<float>(half)), radius));
        }
        return;
    }
    case Object::Shape::Polygon:
    case Object::Shape::Polyline:
        for (const math::Vec2 point : object.points) {
            place(origin + point);
        }
        return;
    }
}

void MapQuery::getOutlines(std::string_view layer, std::vector<std::vector<math::Vec2>>& outlines) const {
    outlines.clear();
    std::vector<math::Vec2> points;
    // clang-format off
    forEachObject(layer, [&](const Object& object, math::Vec2 offset) {
        getOutline(object, offset, points);
        if (object.shape != Object::Shape::Polyline && points.size() >= 3) {
            outlines.push_back(points);
        }
    });
    // clang-format on
}

std::optional<MapQuery::ObjectHit> MapQuery::castObjects(std::string_view layer, const math::Ray& ray) const {
    std::optional<ObjectHit> closest;
    std::vector<math::Vec2> points;
    // clang-format off
    forEachObject(layer, [&](const Object& object, math::Vec2 offset) {
        getOutline(object, offset, points);
        const std::optional<math::RayHit> hit = object.shape == Object::Shape::Polyline ? math::Raycast::chain(ray, points, false) : math::Raycast::polygon(ray, points);
        if (hit && (!closest || hit->distance < closest->distance)) {
            closest = ObjectHit{.object = &object, .point = hit->point, .normal = hit->normal, .distance = hit->distance};
        }
    });
    // clang-format on
    return closest;
}

math::Vec2 MapQuery::worldToCell(math::Vec2 point) const noexcept {
    const math::Vec2 tile = map.tileSize;
    if (map.orientation == Map::Orientation::Isometric) {
        const float across = (point.x - static_cast<float>(map.height) * tile.x * 0.5F) / (tile.x * 0.5F);
        const float down = point.y / (tile.y * 0.5F);
        return {(across + down) * 0.5F, (down - across) * 0.5F};
    }
    if (map.orientation == Map::Orientation::Oblique) {
        const float shearX = map.skew.x / tile.y;
        const float shearY = map.skew.y / tile.x;
        const float determinant = 1.0F - shearX * shearY;
        return {(point.x - shearX * point.y) / determinant / tile.x, (point.y - shearY * point.x) / determinant / tile.y};
    }
    return {point.x / tile.x, point.y / tile.y};
}

math::Vec2 MapQuery::cellToWorldVector(math::Vec2 vector) const noexcept {
    // Isometric object coordinates measure both cell axes in tile heights, and the other orientations measure them in tile sizes.
    const math::Vec2 scale = map.orientation == Map::Orientation::Isometric ? math::Vec2{map.tileSize.y, map.tileSize.y} : map.tileSize;
    return map.objectToWorld(vector * scale) - map.objectToWorld({});
}

std::optional<MapQuery::TileHit> MapQuery::castTiles(std::string_view layer, const math::Ray& ray, const SolidTile& solid) const {
    if (map.orientation == Map::Orientation::Staggered || map.orientation == Map::Orientation::Hexagonal) {
        throw std::invalid_argument("Tile ray casts need an orthogonal, isometric or oblique map.");
    }
    const std::optional<std::pair<const Layer*, math::Vec2>> found = findLayer(layer);
    if (!found || found->first->kind != Layer::Kind::Tile) {
        throw std::invalid_argument("The map has no tile layer named '" + std::string(layer) + "'.");
    }

    // The ray crosses the layer in cell coordinates, where every cell is a unit square, and the hit maps back to the world.
    const Layer& tiles = *found->first;
    const math::Ray cellRay = math::Ray::between(worldToCell(ray.origin - found->second), worldToCell(ray.getEnd() - found->second));
    // clang-format off
    const std::optional<spatial2d::GridRay::Hit> hit = spatial2d::GridRay::cast(cellRay, {1.0F, 1.0F}, [&](spatial2d::Cell cell) {
        const std::uint32_t gid = tiles.getGid(cell.x, cell.y);
        return gid != 0 && (!solid || solid(gid));
    });
    // clang-format on
    if (!hit) {
        return std::nullopt;
    }

    const float distance = cellRay.length > 0.0F ? hit->distance / cellRay.length * ray.length : 0.0F;
    math::Vec2 normal;
    if (!hit->normal.isZero()) {
        normal = cellToWorldVector({hit->normal.y, -hit->normal.x}).getPerpendicular().getNormalized();
        if (math::Vec2::dot(normal, ray.direction) > 0.0F) {
            normal = -normal;
        }
    }
    return TileHit{.column = hit->cell.x, .row = hit->cell.y, .gid = tiles.getGid(hit->cell.x, hit->cell.y), .point = ray.at(distance), .normal = normal, .distance = distance};
}

} // namespace haylen::tiled
