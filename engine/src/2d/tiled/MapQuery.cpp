#include "haylen/2d/tiled/MapQuery.hpp"

#include <array>
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

math::Vec2 MapQuery::alignmentPivot(std::string_view alignment, Map::Orientation orientation) noexcept {
    if (alignment == "topleft") {
        return {0.0F, 0.0F};
    }
    if (alignment == "top") {
        return {0.5F, 0.0F};
    }
    if (alignment == "topright") {
        return {1.0F, 0.0F};
    }
    if (alignment == "left") {
        return {0.0F, 0.5F};
    }
    if (alignment == "center") {
        return {0.5F, 0.5F};
    }
    if (alignment == "right") {
        return {1.0F, 0.5F};
    }
    if (alignment == "bottom") {
        return {0.5F, 1.0F};
    }
    if (alignment == "bottomright") {
        return {1.0F, 1.0F};
    }
    if (alignment == "bottomleft") {
        return {0.0F, 1.0F};
    }
    return orientation == Map::Orientation::Isometric ? math::Vec2{0.5F, 1.0F} : math::Vec2{0.0F, 1.0F};
}

math::Rect MapQuery::getCellBounds(const Layer& layer) noexcept {
    if (layer.chunks.empty()) {
        return {0.0F, 0.0F, static_cast<float>(layer.width), static_cast<float>(layer.height)};
    }
    const auto cellsOf = [](const Layer::Chunk& chunk) { return math::Rect{static_cast<float>(chunk.x), static_cast<float>(chunk.y), static_cast<float>(chunk.width), static_cast<float>(chunk.height)}; };
    math::Rect bounds = cellsOf(layer.chunks.front());
    for (const Layer::Chunk& chunk : layer.chunks) {
        bounds = bounds.merged(cellsOf(chunk));
    }
    return bounds;
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

void MapQuery::forEachObject(std::string_view layer, const ObjectVisitor& visit) const {
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

void MapQuery::traceOutline(const Object& object, std::vector<math::Vec2>& points) const {
    points.clear();
    const math::Vec2 origin = object.position;
    const auto place = [&](math::Vec2 point) { points.push_back(rotateAround(point, origin, object.rotation)); };
    // clang-format off
    const auto placeBox = [&](math::Vec2 top, math::Vec2 size) {
        for (const math::Vec2 corner : {top, top + math::Vec2{size.x, 0.0F}, top + size, top + math::Vec2{0.0F, size.y}}) {
            place(corner);
        }
    };
    // clang-format on
    const float tau = 2.0F * std::numbers::pi_v<float>;

    switch (object.shape) {
    case Object::Shape::Point:
    case Object::Shape::Text:
        return;
    case Object::Shape::Rectangle:
        placeBox(origin, object.size);
        return;
    case Object::Shape::Tile: {
        const Map::TilesetReference* reference = map.findTileset(object.gid);
        if (reference == nullptr) {
            throw std::invalid_argument("A tile object uses the tile '" + std::to_string(Map::tileId(object.gid)) + "', which no tileset of the map holds.");
        }
        const Tileset& tileset = *reference->tileset;
        const math::Vec2 size = object.size.isZero() ? tileset.getSource(Map::tileId(object.gid) - reference->firstGid).getSize() : object.size;
        placeBox(origin - size * alignmentPivot(tileset.objectAlignment, map.orientation), size);
        return;
    }
    case Object::Shape::Ellipse:
    case Object::Shape::Capsule: {
        // A capsule as wide as it is tall is a circle.
        if (object.shape == Object::Shape::Ellipse || object.size.x == object.size.y) {
            const math::Vec2 radii = object.size * 0.5F;
            for (int segment = 0; segment < kCurveSegments; ++segment) {
                const float angle = tau * static_cast<float>(segment) / static_cast<float>(kCurveSegments);
                place(origin + radii + math::Vec2{std::cos(angle) * radii.x, std::sin(angle) * radii.y});
            }
            return;
        }

        // Round caps close the two short sides, around centers one radius in from them.
        const bool wide = object.size.x > object.size.y;
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

void MapQuery::getOutline(const Object& object, math::Vec2 offset, std::vector<math::Vec2>& points) const {
    traceOutline(object, points);

    // Tile objects draw upright around the world position of their anchor, while every other shape follows the map projection point by point.
    const math::Vec2 anchor = map.objectToWorld(object.position);
    for (math::Vec2& point : points) {
        point = (object.shape == Object::Shape::Tile ? anchor + (point - object.position) : map.objectToWorld(point)) + offset;
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

    // The ray crosses the layer in cell coordinates, where every cell is a unit square, and only between the sides of the cells the layer holds, so a ray of any length walks at most across the layer.
    const Layer& tiles = *found->first;
    const math::Vec2 start = worldToCell(ray.origin - found->second);
    const math::Vec2 step = worldToCell(ray.direction) - worldToCell({});
    const float cellsPerUnit = step.getLength();
    const math::Ray cellRay{start, step / cellsPerUnit, ray.length * cellsPerUnit};
    const math::Rect area = getCellBounds(tiles);
    const std::optional<math::RayHit> entry = math::Raycast::rect(cellRay, area);
    const std::optional<std::array<float, 2>> travel = math::Raycast::clip(cellRay, area);
    if (!entry || !travel) {
        return std::nullopt;
    }

    // The walk gathers the tiles first, so the solid callback runs after it and may even change the map.
    std::vector<CrossedTile> crossed;
    const math::Ray inside{entry->point, cellRay.direction, (*travel)[1] - (*travel)[0]};
    // clang-format off
    spatial2d::GridRay::traverse(inside, {1.0F, 1.0F}, [&](spatial2d::Cell cell, float distance, math::Vec2 normal) {
        if (const std::uint32_t gid = tiles.getGid(cell.x, cell.y); gid != 0) {
            crossed.push_back({.column = cell.x, .row = cell.y, .gid = gid, .distance = entry->distance + distance, .normal = normal.isZero() ? entry->normal : normal});
        }
        return true;
    });
    // clang-format on

    for (const CrossedTile& tile : crossed) {
        if (solid && !solid(tile.gid)) {
            continue;
        }
        const float distance = tile.distance / cellsPerUnit;
        math::Vec2 normal;
        if (!tile.normal.isZero()) {
            normal = cellToWorldVector({tile.normal.y, -tile.normal.x}).getPerpendicular().getNormalized();
            if (math::Vec2::dot(normal, ray.direction) > 0.0F) {
                normal = -normal;
            }
        }
        return TileHit{.column = tile.column, .row = tile.row, .gid = tile.gid, .point = ray.at(distance), .normal = normal, .distance = distance};
    }
    return std::nullopt;
}

} // namespace haylen::tiled
