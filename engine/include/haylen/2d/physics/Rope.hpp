#pragma once

#include <optional>
#include <vector>

#include "haylen/2d/physics/Body.hpp"
#include "haylen/2d/physics/CollisionFilter.hpp"
#include "haylen/2d/physics/Joint.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::physics2d {

class World;

// A chain of segments from start to end joined by revolute joints, such as a rope, a chain or a rope bridge. Each end can hang from a body or be pinned in place. The handles stay valid until the bodies are destroyed, by `destroy` or otherwise.
class Rope final {
  public:
    // Segments are capsules, or boxes with `planks` set. An end with a body hangs from it at the end point, and a pinned end without a body hangs from a static anchor that the rope creates and destroys.
    struct Options {
        math::Vec2 start{};
        math::Vec2 end{};
        int segments = 10;
        float thickness = 4.0F;
        float density = 1.0F;
        float friction = 0.6F;
        float linearDamping = 0.0F;
        float angularDamping = 0.5F;
        bool planks = false;
        bool pinStart = false;
        bool pinEnd = false;
        std::optional<Body> startBody;
        std::optional<Body> endBody;
        CollisionFilter filter{};
    };

    // The position is the center of the segment, which is where a sprite for it goes.
    struct Segment {
        math::Vec2 position{};
        float rotation = 0.0F;
        float length = 0.0F;
    };

    // Throws `std::invalid_argument` when there are no segments, the ends coincide, the thickness is not positive, the material or damping is invalid or an end body is not a live body of the world, and then leaves no bodies behind.
    [[nodiscard]] static Rope create(World& world, const Options& options);

    // Creates a bridge of planks pinned at both ends unless the options give bodies for them.
    [[nodiscard]] static Rope createBridge(World& world, Options options);

    [[nodiscard]] const std::vector<Body>& getBodies() const noexcept {
        return bodies;
    }
    [[nodiscard]] const std::vector<Joint>& getJoints() const noexcept {
        return joints;
    }
    [[nodiscard]] float getSegmentLength() const noexcept {
        return segmentLength;
    }
    [[nodiscard]] bool isValid() const noexcept;

    [[nodiscard]] std::vector<Segment> getSegments() const;
    // Returns the ends of the segments in order, from the start point to the end point, which draws the rope as a line.
    [[nodiscard]] std::vector<math::Vec2> getPoints() const;

    // Destroys the segments, their joints and the anchors the rope created.
    void destroy();

  private:
    void build(World& world, const Options& options);

    std::vector<Body> bodies;
    std::vector<Body> anchors;
    std::vector<Joint> joints;
    float segmentLength = 0.0F;
};

} // namespace haylen::physics2d
