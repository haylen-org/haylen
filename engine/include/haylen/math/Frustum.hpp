#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include "haylen/math/Box3.hpp"
#include "haylen/math/Mat4.hpp"
#include "haylen/math/Plane.hpp"
#include "haylen/math/Sphere.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::math {

// The space that a camera sees, bounded by the planes of its view and projection, whose normals point inside.
class Frustum final {
  public:
    enum class Containment : std::uint8_t {
        Outside,
        Intersecting,
        Inside,
    };

    // How far outside a plane a point may lie and still count as inside, in the units of the world, so a shape that rests on a plane is never cut by rounding.
    static constexpr float kTolerance = 1e-4F;

    // Returns the frustum of `projection * view`, for the range of depth the projection was built with. A projection without a far end gives five planes.
    [[nodiscard]] static Frustum fromMatrix(const Mat4& viewProjection, Mat4::DepthRange depthRange);

    [[nodiscard]] static std::optional<Containment> containmentFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view containmentName(Containment value) noexcept;

    // Returns the planes in the order left, right, bottom, top, near and far.
    [[nodiscard]] std::span<const Plane> getPlanes() const noexcept {
        return std::span<const Plane>(planes).first(planeCount);
    }

    [[nodiscard]] bool contains(Vec3 point, float tolerance = kTolerance) const noexcept;

    // Tells whether a shape lies wholly inside, crosses a plane or lies wholly outside. The test never calls a shape outside that reaches inside, while a box near a corner of the frustum may count as crossing though it lies outside.
    [[nodiscard]] Containment classify(const Sphere& sphere, float tolerance = kTolerance) const noexcept;
    [[nodiscard]] Containment classify(const Box3& box, float tolerance = kTolerance) const noexcept;

  private:
    static const std::array<std::pair<std::string_view, Containment>, 3> kContainmentNames;

    [[nodiscard]] Containment classify(Vec3 center, Vec3 halfExtents, float radius, float tolerance) const noexcept;

    std::array<Plane, 6> planes{};
    std::size_t planeCount = 0;
};

} // namespace haylen::math
