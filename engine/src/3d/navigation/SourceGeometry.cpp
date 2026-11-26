#include "haylen/3d/navigation/SourceGeometry.hpp"

#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string>

namespace haylen::navigation3d {

SourceGeometry::SourceGeometry() : areas{std::string(kGround)}, bounds{math::Vec3{std::numeric_limits<float>::max()}, math::Vec3{std::numeric_limits<float>::lowest()}} {}

math::Vec3 SourceGeometry::requireFinite(math::Vec3 point, std::string_view what) {
    const bool near = std::fabs(point.x) <= kMaxCoordinate && std::fabs(point.y) <= kMaxCoordinate && std::fabs(point.z) <= kMaxCoordinate;
    if (!point.isFinite() || !near) {
        throw std::invalid_argument("Every " + std::string(what) + " of the geometry of a navigation mesh must be a finite number within a trillion meters of the origin.");
    }

    return point;
}

std::uint8_t SourceGeometry::requireArea(std::string_view name) {
    if (name == kNone) {
        return kNoneArea;
    }

    for (std::size_t index = 0; index < areas.size(); ++index) {
        if (areas[index] == name) {
            return static_cast<std::uint8_t>(index + 1);
        }
    }

    if (name.empty()) {
        throw std::invalid_argument("The name of an area of a navigation mesh cannot be empty.");
    }
    if (areas.size() == kMaxAreas) {
        throw std::invalid_argument("A navigation mesh holds at most 63 areas besides \"none\", so the area \"" + std::string(name) + "\" does not fit.");
    }

    areas.emplace_back(name);
    return static_cast<std::uint8_t>(areas.size());
}

void SourceGeometry::reserve(std::size_t vertexCount, std::size_t triangleCount) {
    if (vertices.size() / 3 + vertexCount > kMaxVertices) {
        throw std::invalid_argument("The geometry of a navigation mesh holds at most 268435456 vertices.");
    }

    vertices.reserve(vertices.size() + vertexCount * 3);
    triangles.reserve(triangles.size() + triangleCount * 3);
    triangleAreas.reserve(triangleAreas.size() + triangleCount);
}

std::int32_t SourceGeometry::addVertex(math::Vec3 point) {
    const auto index = static_cast<std::int32_t>(vertices.size() / 3);
    vertices.push_back(point.x);
    vertices.push_back(point.y);
    vertices.push_back(point.z);
    bounds = bounds.merged(point);
    return index;
}

void SourceGeometry::addTriangle(std::int32_t a, std::int32_t b, std::int32_t c, std::uint8_t area) {
    triangles.push_back(a);
    triangles.push_back(b);
    triangles.push_back(c);
    triangleAreas.push_back(area);
}

void SourceGeometry::addMesh(std::span<const math::Vec3> positions, std::span<const std::uint32_t> indices, const math::Mat4& placement, std::string_view area) {
    if (!placement.isAffine()) {
        throw std::invalid_argument("The placement of a mesh of the geometry of a navigation mesh must be an affine matrix.");
    }
    if (indices.empty() && positions.size() % 3 != 0) {
        throw std::invalid_argument("A mesh without indices needs three positions for each triangle.");
    }
    if (indices.size() % 3 != 0) {
        throw std::invalid_argument("The indices of a mesh need three vertices for each triangle.");
    }
    for (std::size_t item = 0; item < indices.size(); ++item) {
        if (indices[item] >= positions.size()) {
            throw std::invalid_argument("Item " + std::to_string(item + 1) + " of the indices of a mesh names vertex " + std::to_string(indices[item] + 1) + ", but the vertex count of the mesh is " + std::to_string(positions.size()) + ".");
        }
    }

    const std::uint8_t areaId = requireArea(area);
    const std::size_t triangleCount = indices.empty() ? positions.size() / 3 : indices.size() / 3;
    reserve(positions.size(), triangleCount);

    const auto first = static_cast<std::int32_t>(vertices.size() / 3);
    for (const math::Vec3 position : positions) {
        addVertex(requireFinite(placement.apply(requireFinite(position, "position")), "placed position"));
    }

    // A mirrored placement turns the triangles over, so their corners run the other way.
    const bool mirrored = placement.getLinear().getDeterminant() < 0.0F;
    for (std::size_t triangle = 0; triangle < triangleCount; ++triangle) {
        const std::size_t base = triangle * 3;
        const auto a = static_cast<std::int32_t>(indices.empty() ? base : indices[base]);
        const auto b = static_cast<std::int32_t>(indices.empty() ? base + 1 : indices[base + 1]);
        const auto c = static_cast<std::int32_t>(indices.empty() ? base + 2 : indices[base + 2]);
        if (mirrored) {
            addTriangle(first + a, first + c, first + b, areaId);
        } else {
            addTriangle(first + a, first + b, first + c, areaId);
        }
    }
}

void SourceGeometry::addBox(const math::Obb& box, std::string_view area) {
    const std::uint8_t areaId = requireArea(area);
    reserve(8, 12);

    const math::Mat3 axes = box.getAxes();
    const math::Vec3 x = axes.getColumn(0) * std::fabs(box.halfExtents.x);
    const math::Vec3 y = axes.getColumn(1) * std::fabs(box.halfExtents.y);
    const math::Vec3 z = axes.getColumn(2) * std::fabs(box.halfExtents.z);
    std::array<std::int32_t, 8> corners{};
    for (std::size_t corner = 0; corner < corners.size(); ++corner) {
        const float sx = (corner & 1U) != 0 ? 1.0F : -1.0F;
        const float sy = (corner & 2U) != 0 ? 1.0F : -1.0F;
        const float sz = (corner & 4U) != 0 ? 1.0F : -1.0F;
        corners[corner] = addVertex(requireFinite(box.center + x * sx + y * sy + z * sz, "corner of a box"));
    }

    // Each face lists its corners counter-clockwise seen from outside the box.
    static constexpr std::array<std::array<std::size_t, 4>, 6> kFaces{{{0, 4, 6, 2}, {1, 3, 7, 5}, {0, 1, 5, 4}, {2, 6, 7, 3}, {0, 2, 3, 1}, {4, 5, 7, 6}}};
    for (const std::array<std::size_t, 4>& face : kFaces) {
        addTriangle(corners[face[0]], corners[face[1]], corners[face[2]], areaId);
        addTriangle(corners[face[0]], corners[face[2]], corners[face[3]], areaId);
    }
}

void SourceGeometry::addRevolution(math::Vec3 start, math::Vec3 end, std::span<const float> offsets, std::span<const float> radii, std::uint8_t area) {
    const math::Vec3 axis = requireFinite(end - start, "axis of a shape");
    const float length = axis.getLength();
    const math::Vec3 up = length > 0.0F ? axis / length : math::Vec3::up();
    const math::Vec3 helper = std::fabs(up.y) < 0.9F ? math::Vec3::up() : math::Vec3::right();
    const math::Vec3 side = math::Vec3::cross(helper, up).getNormalized();
    const math::Vec3 front = math::Vec3::cross(up, side);

    reserve(offsets.size() * kShapeSegments, offsets.size() * kShapeSegments * 2);
    std::vector<std::int32_t> rings;
    rings.reserve(offsets.size() * kShapeSegments);
    for (std::size_t ring = 0; ring < offsets.size(); ++ring) {
        for (std::size_t segment = 0; segment < kShapeSegments; ++segment) {
            const float angle = 2.0F * std::numbers::pi_v<float> * static_cast<float>(segment) / static_cast<float>(kShapeSegments);
            const math::Vec3 around = (side * std::cos(angle) + front * std::sin(angle)) * radii[ring];
            rings.push_back(addVertex(requireFinite(start + up * offsets[ring] + around, "point of a shape")));
        }
    }

    // Neighboring rings join in quads, and the first and last rings close with fans around the ends.
    for (std::size_t ring = 0; ring + 1 < offsets.size(); ++ring) {
        for (std::size_t segment = 0; segment < kShapeSegments; ++segment) {
            const std::size_t next = (segment + 1) % kShapeSegments;
            const std::int32_t a = rings[ring * kShapeSegments + segment];
            const std::int32_t b = rings[ring * kShapeSegments + next];
            const std::int32_t c = rings[(ring + 1) * kShapeSegments + next];
            const std::int32_t d = rings[(ring + 1) * kShapeSegments + segment];
            addTriangle(a, b, c, area);
            addTriangle(a, c, d, area);
        }
    }

    const std::int32_t bottom = addVertex(start);
    const std::int32_t top = addVertex(end);
    const std::size_t last = (offsets.size() - 1) * kShapeSegments;
    for (std::size_t segment = 0; segment < kShapeSegments; ++segment) {
        const std::size_t next = (segment + 1) % kShapeSegments;
        addTriangle(bottom, rings[next], rings[segment], area);
        addTriangle(top, rings[last + segment], rings[last + next], area);
    }
}

void SourceGeometry::addSphere(const math::Sphere& sphere, std::string_view area) {
    if (!(sphere.radius > 0.0F) || !std::isfinite(sphere.radius)) {
        throw std::invalid_argument("The radius of a sphere of the geometry of a navigation mesh must be a finite number above zero.");
    }

    const std::uint8_t areaId = requireArea(area);
    std::array<float, kShapeSegments / 2 - 1> offsets{};
    std::array<float, kShapeSegments / 2 - 1> radii{};
    for (std::size_t ring = 0; ring < offsets.size(); ++ring) {
        const float angle = std::numbers::pi_v<float> * static_cast<float>(ring + 1) / static_cast<float>(offsets.size() + 1);
        offsets[ring] = sphere.radius - std::cos(angle) * sphere.radius;
        radii[ring] = std::sin(angle) * sphere.radius;
    }

    const math::Vec3 center = requireFinite(sphere.center, "center of a sphere");
    addRevolution(center - math::Vec3{0.0F, sphere.radius, 0.0F}, center + math::Vec3{0.0F, sphere.radius, 0.0F}, offsets, radii, areaId);
}

void SourceGeometry::addCapsule(const math::Capsule& capsule, std::string_view area) {
    if (!(capsule.radius > 0.0F) || !std::isfinite(capsule.radius)) {
        throw std::invalid_argument("The radius of a capsule of the geometry of a navigation mesh must be a finite number above zero.");
    }

    const std::uint8_t areaId = requireArea(area);
    const math::Vec3 start = requireFinite(capsule.start, "end of a capsule");
    const math::Vec3 end = requireFinite(capsule.end, "end of a capsule");
    const float length = (end - start).getLength();

    // Each cap is a half sphere of four rings, and the body runs straight between them.
    constexpr std::size_t kCapRings = 4;
    std::array<float, kCapRings * 2> offsets{};
    std::array<float, kCapRings * 2> radii{};
    for (std::size_t ring = 0; ring < kCapRings; ++ring) {
        const float angle = 0.5F * std::numbers::pi_v<float> * static_cast<float>(ring + 1) / static_cast<float>(kCapRings);
        offsets[ring] = capsule.radius - std::cos(angle) * capsule.radius;
        radii[ring] = std::sin(angle) * capsule.radius;
        offsets[kCapRings * 2 - 1 - ring] = capsule.radius + length + std::cos(angle) * capsule.radius;
        radii[kCapRings * 2 - 1 - ring] = radii[ring];
    }

    const math::Vec3 direction = length > 0.0F ? (end - start) / length : math::Vec3::up();
    const math::Vec3 bottom = start - direction * capsule.radius;
    addRevolution(bottom, bottom + direction * (length + capsule.radius * 2.0F), offsets, radii, areaId);
}

void SourceGeometry::addCylinder(math::Vec3 start, math::Vec3 end, float radius, std::string_view area) {
    if (!(radius > 0.0F) || !std::isfinite(radius)) {
        throw std::invalid_argument("The radius of a cylinder of the geometry of a navigation mesh must be a finite number above zero.");
    }

    const std::uint8_t areaId = requireArea(area);
    const float length = (requireFinite(end, "end of a cylinder") - requireFinite(start, "end of a cylinder")).getLength();
    const std::array<float, 2> offsets{0.0F, length};
    const std::array<float, 2> radii{radius, radius};
    addRevolution(start, end, offsets, radii, areaId);
}

void SourceGeometry::addHeightfield(const Heightfield& terrain, std::string_view area) {
    if (terrain.columns < 2 || terrain.rows < 2) {
        throw std::invalid_argument("A heightfield of the geometry of a navigation mesh needs at least two columns and two rows.");
    }
    if (terrain.columns > kMaxHeightfieldSamples / terrain.rows) {
        throw std::invalid_argument("A heightfield of the geometry of a navigation mesh holds at most 67108864 samples.");
    }
    if (terrain.heights.size() != terrain.columns * terrain.rows) {
        throw std::invalid_argument("A heightfield of " + std::to_string(terrain.columns) + " columns and " + std::to_string(terrain.rows) + " rows needs " + std::to_string(terrain.columns * terrain.rows) + " heights, but it holds " + std::to_string(terrain.heights.size()) + ".");
    }
    if (!(terrain.spacing > 0.0F) || !std::isfinite(terrain.spacing)) {
        throw std::invalid_argument("The spacing of a heightfield of the geometry of a navigation mesh must be a finite number above zero.");
    }

    const std::uint8_t areaId = requireArea(area);
    const math::Vec3 origin = requireFinite(terrain.origin, "origin of a heightfield");
    reserve(terrain.heights.size(), (terrain.columns - 1) * (terrain.rows - 1) * 2);

    const auto first = static_cast<std::int32_t>(vertices.size() / 3);
    for (std::size_t row = 0; row < terrain.rows; ++row) {
        for (std::size_t column = 0; column < terrain.columns; ++column) {
            const math::Vec3 sample{static_cast<float>(column) * terrain.spacing, terrain.heights[row * terrain.columns + column], static_cast<float>(row) * terrain.spacing};
            addVertex(requireFinite(origin + sample, "height of a heightfield"));
        }
    }

    // Each cell splits into two triangles whose corners run counter-clockwise seen from above.
    const auto columns = static_cast<std::int32_t>(terrain.columns);
    for (std::int32_t row = 0; row + 1 < static_cast<std::int32_t>(terrain.rows); ++row) {
        for (std::int32_t column = 0; column + 1 < columns; ++column) {
            const std::int32_t corner = first + row * columns + column;
            addTriangle(corner, corner + columns, corner + 1, areaId);
            addTriangle(corner + 1, corner + columns, corner + columns + 1, areaId);
        }
    }
}

void SourceGeometry::addVolume(const Volume& volume) {
    if (volume.outline.size() < 3 || volume.outline.size() > kMaxVolumePoints) {
        throw std::invalid_argument("The outline of a volume of the geometry of a navigation mesh needs from 3 to 256 points.");
    }
    if (!std::isfinite(volume.bottom) || !std::isfinite(volume.top) || volume.top < volume.bottom) {
        throw std::invalid_argument("The top of a volume of the geometry of a navigation mesh must be a finite number at or above its finite bottom.");
    }

    MarkedVolume marked{.outline = {}, .bottom = volume.bottom, .top = volume.top, .area = requireArea(volume.area)};
    marked.outline.reserve(volume.outline.size() * 3);
    for (const math::Vec3 point : volume.outline) {
        requireFinite(point, "point of the outline of a volume");
        marked.outline.push_back(point.x);
        marked.outline.push_back(volume.bottom);
        marked.outline.push_back(point.z);
    }

    volumes.push_back(std::move(marked));
}

void SourceGeometry::requireLink(const Link& link) {
    requireFinite(link.start, "end of a link");
    requireFinite(link.end, "end of a link");
    if (!(link.radius > 0.0F) || link.radius > kMaxCoordinate) {
        throw std::invalid_argument("The radius of a link of a navigation mesh must be a finite number above zero.");
    }
}

void SourceGeometry::addLink(const Link& link) {
    requireLink(link);
    requireArea(link.area);
    links.push_back(link);
}

} // namespace haylen::navigation3d
