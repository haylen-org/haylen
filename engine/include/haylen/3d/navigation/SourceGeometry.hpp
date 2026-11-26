#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/3d/navigation/Link.hpp"
#include "haylen/math/Box.hpp"
#include "haylen/math/Capsule.hpp"
#include "haylen/math/Mat4.hpp"
#include "haylen/math/Obb.hpp"
#include "haylen/math/Sphere.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::navigation3d {

// The world that a navigation mesh bakes from: triangles of meshes, the shapes that physics uses, terrain, volumes that mark areas and links across gaps, in meters. Triangles whose corners run counter-clockwise seen from above face up and can be walked on, as every front face of the engine does. Each piece names its area, such as `road` or `water`, which query filters give a cost. The area `ground` is the default, `none` is never walked, and the other names take the next free area as they first appear, so a mesh holds up to 63 areas besides `none`. Where two surfaces meet within a step, the area that appeared later wins.
class SourceGeometry final {
  public:
    // Terrain as a grid of heights, `columns` along x and `rows` along z, row by row from the smallest z, with `spacing` meters between samples and `origin` at the sample of the smallest x and z, whose height adds to every sample.
    struct Heightfield {
        math::Vec3 origin;
        std::size_t columns = 0;
        std::size_t rows = 0;
        float spacing = 1.0F;
        std::vector<float> heights;
    };

    // A convex outline on the ground, from `bottom` to `top`, that marks its area on the walkable surface inside it, such as water, a road or, with the area `none`, a hole. The heights of the outline points are not used.
    struct Volume {
        std::vector<math::Vec3> outline;
        float bottom = 0.0F;
        float top = 0.0F;
        std::string area = "ground";
    };

    static constexpr std::string_view kGround = "ground";
    static constexpr std::string_view kNone = "none";
    static constexpr std::uint8_t kNoneArea = 0;
    static constexpr std::uint8_t kGroundArea = 1;
    static constexpr std::size_t kMaxAreas = 63;
    static constexpr std::size_t kMaxVertices = std::size_t{1} << 28U;
    static constexpr std::size_t kMaxHeightfieldSamples = std::size_t{1} << 26U;
    static constexpr std::size_t kMaxVolumePoints = 256;
    static constexpr std::size_t kShapeSegments = 16;

    // The farthest a point lies from the origin on each axis, so the squares of the distances of a mesh stay within floats.
    static constexpr float kMaxCoordinate = 1.0e12F;

    SourceGeometry();

    // Adds the triangles of a mesh placed by an affine matrix. The `indices` name three positions for each triangle, and without them the positions list every triangle in turn. A matrix that mirrors turns the triangles over, so they keep facing out.
    void addMesh(std::span<const math::Vec3> positions, std::span<const std::uint32_t> indices, const math::Mat4& placement = math::Mat4::identity(), std::string_view area = kGround);

    // These add the surface of a shape of physics, cut into triangles. A cylinder runs between the centers of its caps.
    void addBox(const math::Obb& box, std::string_view area = kGround);
    void addSphere(const math::Sphere& sphere, std::string_view area = kGround);
    void addCapsule(const math::Capsule& capsule, std::string_view area = kGround);
    void addCylinder(math::Vec3 start, math::Vec3 end, float radius, std::string_view area = kGround);

    void addHeightfield(const Heightfield& terrain, std::string_view area = kGround);
    void addVolume(const Volume& volume);
    void addLink(const Link& link);

    // Throws `std::invalid_argument` for a link whose ends are not finite or lie too far away, or whose radius is not a finite number above zero, which a mesh checks for its links too.
    static void requireLink(const Link& link);

    // Returns the area of a name, adding the name when it is new. Throws `std::invalid_argument` past the areas a mesh holds.
    std::uint8_t requireArea(std::string_view name);

    // The names of the areas from area 1 on, where `ground` comes first.
    [[nodiscard]] const std::vector<std::string>& getAreas() const noexcept {
        return areas;
    }
    [[nodiscard]] const math::Box& getBounds() const noexcept {
        return bounds;
    }
    [[nodiscard]] bool isEmpty() const noexcept {
        return triangles.empty();
    }

    // The positions of the vertices, three numbers each, and the vertices of each triangle with its area, in the form the baker reads.
    [[nodiscard]] const std::vector<float>& getVertices() const noexcept {
        return vertices;
    }
    [[nodiscard]] const std::vector<std::int32_t>& getTriangles() const noexcept {
        return triangles;
    }
    [[nodiscard]] const std::vector<std::uint8_t>& getTriangleAreas() const noexcept {
        return triangleAreas;
    }
    [[nodiscard]] std::size_t getTriangleCount() const noexcept {
        return triangleAreas.size();
    }

    // The volumes with their area, and the links with their area and kind.
    struct MarkedVolume {
        std::vector<float> outline;
        float bottom = 0.0F;
        float top = 0.0F;
        std::uint8_t area = kGroundArea;
    };
    [[nodiscard]] const std::vector<MarkedVolume>& getVolumes() const noexcept {
        return volumes;
    }
    [[nodiscard]] const std::vector<Link>& getLinks() const noexcept {
        return links;
    }

  private:
    static math::Vec3 requireFinite(math::Vec3 point, std::string_view what);
    void reserve(std::size_t vertexCount, std::size_t triangleCount);
    std::int32_t addVertex(math::Vec3 point);
    void addTriangle(std::int32_t a, std::int32_t b, std::int32_t c, std::uint8_t area);

    // Adds the triangles of a surface of revolution around the segment from `start` to `end`, from rings of points at the offsets from `start` along it and the radii around it, closed by fans around both ends.
    void addRevolution(math::Vec3 start, math::Vec3 end, std::span<const float> offsets, std::span<const float> radii, std::uint8_t area);

    std::vector<std::string> areas;
    std::vector<float> vertices;
    std::vector<std::int32_t> triangles;
    std::vector<std::uint8_t> triangleAreas;
    std::vector<MarkedVolume> volumes;
    std::vector<Link> links;
    math::Box bounds;
};

} // namespace haylen::navigation3d
