#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/Shape/Shape.h>

#include <memory>
#include <string_view>
#include <vector>

#include "haylen/3d/physics/Shape.hpp"

namespace haylen::physics3d::jolt {

class Shape;

// Builds the shapes of Jolt from settings that `Shape::Settings::validate` checked, on any thread. Round shapes stand along `y` around their middle, and a tapered capsule around the middle between the centers of its caps.
class ShapeBuilder final {
  public:
    // Throws `std::invalid_argument` with a sentence that says what to change when Jolt cannot hold the shape, such as a mesh too large for it.
    [[nodiscard]] static std::shared_ptr<const Shape> build(const physics3d::Shape::Settings& settings);

  private:
    // Makes the shape of the settings, or throws `std::invalid_argument` with the sentence.
    [[nodiscard]] static JPH::RefConst<JPH::Shape> make(const JPH::ShapeSettings& settings, std::string_view failure);

    [[nodiscard]] static JPH::RefConst<JPH::Shape> makeHull(const physics3d::Shape::ConvexHull& hull, float density);
    [[nodiscard]] static JPH::RefConst<JPH::Shape> makeMesh(const physics3d::Shape::Mesh& mesh);
    [[nodiscard]] static JPH::RefConst<JPH::Shape> makeHeightField(const physics3d::Shape::HeightField& field);
    [[nodiscard]] static JPH::RefConst<JPH::Shape> makeStaticCompound(const std::vector<physics3d::Shape::Child>& children);
    [[nodiscard]] static JPH::RefConst<JPH::Shape> makeMutableCompound(const std::vector<physics3d::Shape::Child>& children);
};

} // namespace haylen::physics3d::jolt
