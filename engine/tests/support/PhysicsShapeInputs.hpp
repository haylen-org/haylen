#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "3d/physics/jolt/Backend.hpp"
#include "haylen/3d/physics/Shape.hpp"

namespace haylen::test {

// Turns bytes into the data of a convex hull, a triangle mesh or a heightfield, by the first byte, and makes the 3D physics shape of that data, so the shapes that take data of any size meet every value a file or a script could hand them. It returns the sentence that refuses the data, or nothing once the shape is made, and anything else, a crash, a trap of the physics or another exception, is a bug. The fuzzer of the shapes and the tests of hostile shape data both drive it.
class PhysicsShapeInputs final {
  public:
    [[nodiscard]] static std::optional<std::string> build(std::span<const std::uint8_t> input);

  private:
    // The first half of the floats are positions and the rest are indices, so meshes of every size and every index meet the checks.
    [[nodiscard]] static physics3d::Shape::Mesh readMesh(std::span<const std::uint8_t> body);

    // Every float after the first byte is a height. An even first byte makes the largest grid that the heights fill, and an odd one names the side of the grid itself, so both grids that fill and grids that do not meet the checks.
    [[nodiscard]] static physics3d::Shape::HeightField readHeightField(std::span<const std::uint8_t> body);

    [[nodiscard]] static std::vector<math::Vec3> readPoints(std::span<const std::uint8_t> body);
    [[nodiscard]] static std::vector<float> readFloats(std::span<const std::uint8_t> body);

    static inline physics3d::jolt::Backend backend;
};

} // namespace haylen::test
