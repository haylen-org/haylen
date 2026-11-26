#include "3d/navigation/RandomPoints.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace haylen::navigation3d {

RandomPoints::Outline RandomPoints::getOutline(const dtMeshTile& tile, const dtPoly& poly) {
    Outline outline;
    for (unsigned int corner = 0; corner < poly.vertCount; ++corner) {
        const float* vertex = &tile.verts[poly.verts[corner] * 3];
        outline.points[outline.count++] = {vertex[0], vertex[2]};
    }

    // The outline runs counter-clockwise in the plane of x and z, which the cut by the circle expects.
    if (getArea(outline) < 0.0F) {
        std::reverse(outline.points.begin(), outline.points.begin() + static_cast<std::ptrdiff_t>(outline.count));
    }

    return outline;
}

float RandomPoints::getArea(const Outline& outline) {
    float twice = 0.0F;
    for (std::size_t corner = 0; corner < outline.count; ++corner) {
        const math::Vec2 a = outline.points[corner];
        const math::Vec2 b = outline.points[(corner + 1) % outline.count];
        twice += a.x * b.y - b.x * a.y;
    }

    return twice * 0.5F;
}

RandomPoints::Outline RandomPoints::clip(const Outline& outline, math::Vec2 center, float radius) {
    // The circle becomes the polygon of its sides inside it, so every point of the cut lies within the radius.
    Outline current = outline;
    for (std::size_t side = 0; side < kCircleSides; ++side) {
        const float from = 2.0F * std::numbers::pi_v<float> * static_cast<float>(side) / static_cast<float>(kCircleSides);
        const float to = 2.0F * std::numbers::pi_v<float> * static_cast<float>(side + 1) / static_cast<float>(kCircleSides);
        const math::Vec2 a = center + math::Vec2{std::cos(from), std::sin(from)} * radius;
        const math::Vec2 b = center + math::Vec2{std::cos(to), std::sin(to)} * radius;
        const math::Vec2 edge = b - a;

        Outline kept;
        for (std::size_t corner = 0; corner < current.count; ++corner) {
            const math::Vec2 p = current.points[corner];
            const math::Vec2 q = current.points[(corner + 1) % current.count];
            const float pSide = math::Vec2::cross(edge, p - a);
            const float qSide = math::Vec2::cross(edge, q - a);
            if (pSide >= 0.0F) {
                kept.points[kept.count++] = p;
            }
            if ((pSide >= 0.0F) != (qSide >= 0.0F)) {
                kept.points[kept.count++] = p + (q - p) * (pSide / (pSide - qSide));
            }
        }

        current = kept;
        if (current.count < 3) {
            return {};
        }
    }

    return current;
}

math::Vec2 RandomPoints::pickInside(const Outline& outline, math::Random& random) {
    // A fan of triangles from the first corner, one picked by its area and a point picked evenly inside it.
    const float total = getArea(outline);
    float remaining = random.nextFloat() * total;
    std::size_t triangle = 1;
    for (; triangle + 2 < outline.count; ++triangle) {
        const float area = math::Vec2::cross(outline.points[triangle] - outline.points[0], outline.points[triangle + 1] - outline.points[0]) * 0.5F;
        if (remaining < area) {
            break;
        }
        remaining -= area;
    }

    float u = random.nextFloat();
    float v = random.nextFloat();
    if (u + v > 1.0F) {
        u = 1.0F - u;
        v = 1.0F - v;
    }

    const math::Vec2 origin = outline.points[0];
    return origin + (outline.points[triangle] - origin) * u + (outline.points[triangle + 1] - origin) * v;
}

std::optional<RandomPoints::Point> RandomPoints::place(const dtNavMeshQuery& query, dtPolyRef polygon, math::Vec2 ground, float height) {
    Point point{.position = {ground.x, height, ground.y}, .polygon = polygon};
    float surface = height;
    if (dtStatusFailed(query.getPolyHeight(polygon, point.position.data(), &surface))) {
        return std::nullopt;
    }

    point.position[1] = surface;
    return point;
}

std::optional<RandomPoints::Point> RandomPoints::onMesh(const dtNavMesh& mesh, const dtNavMeshQuery& query, const dtQueryFilter& filter, math::Random& random) {
    // The first walk sums the areas of the polygons that the filter passes, and the second one stops at the polygon of the picked share.
    float total = 0.0F;
    for (int pass = 0; pass < 2; ++pass) {
        const float target = random.nextFloat() * total;
        float sum = 0.0F;
        for (int index = 0; index < mesh.getMaxTiles(); ++index) {
            const dtMeshTile* tile = mesh.getTile(index);
            if (tile->header == nullptr) {
                continue;
            }

            const dtPolyRef base = mesh.getPolyRefBase(tile);
            for (int polygon = 0; polygon < tile->header->polyCount; ++polygon) {
                const dtPoly& poly = tile->polys[polygon];
                const dtPolyRef reference = base | static_cast<dtPolyRef>(polygon);
                if (poly.getType() != DT_POLYTYPE_GROUND || !filter.passFilter(reference, tile, &poly)) {
                    continue;
                }

                const Outline outline = getOutline(*tile, poly);
                const float area = getArea(outline);
                sum += area;
                if (pass == 1 && area > 0.0F && sum >= target) {
                    return place(query, reference, pickInside(outline, random), tile->verts[poly.verts[0] * 3 + 1]);
                }
            }
        }

        if (sum <= 0.0F) {
            return std::nullopt;
        }
        total = sum;
    }

    return std::nullopt;
}

std::optional<RandomPoints::Point> RandomPoints::around(const dtNavMesh& mesh, const dtNavMeshQuery& query, const dtQueryFilter& filter, dtPolyRef start, const std::array<float, 3>& center, float radius, math::Random& random, Buffers& buffers) {
    buffers.polygons.resize(kMaxAround);
    buffers.parents.resize(kMaxAround);
    int count = 0;
    if (dtStatusFailed(query.findPolysAroundCircle(start, center.data(), radius, &filter, buffers.polygons.data(), buffers.parents.data(), nullptr, &count, static_cast<int>(kMaxAround)))) {
        return std::nullopt;
    }

    // Every polygon that the center reaches counts with the part of it inside the circle.
    const math::Vec2 ground{center[0], center[2]};
    buffers.areas.assign(static_cast<std::size_t>(count), 0.0F);
    float total = 0.0F;
    for (std::size_t index = 0; index < buffers.areas.size(); ++index) {
        const dtMeshTile* tile = nullptr;
        const dtPoly* poly = nullptr;
        mesh.getTileAndPolyByRefUnsafe(buffers.polygons[index], &tile, &poly);
        if (poly->getType() == DT_POLYTYPE_GROUND) {
            buffers.areas[index] = getArea(clip(getOutline(*tile, *poly), ground, radius));
            total += buffers.areas[index];
        }
    }
    if (total <= 0.0F) {
        return std::nullopt;
    }

    // The last polygon with an area takes what rounding leaves over.
    float remaining = random.nextFloat() * total;
    std::size_t chosen = 0;
    for (std::size_t index = 0; index < buffers.areas.size(); ++index) {
        if (buffers.areas[index] <= 0.0F) {
            continue;
        }

        chosen = index;
        if (remaining < buffers.areas[index]) {
            break;
        }
        remaining -= buffers.areas[index];
    }

    const dtMeshTile* tile = nullptr;
    const dtPoly* poly = nullptr;
    mesh.getTileAndPolyByRefUnsafe(buffers.polygons[chosen], &tile, &poly);
    return place(query, buffers.polygons[chosen], pickInside(clip(getOutline(*tile, *poly), ground, radius), random), center[1]);
}

} // namespace haylen::navigation3d
