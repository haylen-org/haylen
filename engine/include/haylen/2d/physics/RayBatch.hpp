#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "haylen/2d/physics/RaycastHit.hpp"
#include "haylen/math/Segment.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::physics2d {

class Raycaster;

// Rays cast together, with a result slot for each one. The batch keeps its buffers, so casting the same number of rays again reuses them, and results line up with the rays by index.
class RayBatch final {
  public:
    // Sets the number of rays, keeping the existing ones and adding rays of zero length.
    void resize(std::size_t count);
    [[nodiscard]] std::size_t size() const noexcept {
        return rays.size();
    }

    void setRay(std::size_t index, math::Vec2 from, math::Vec2 to);
    [[nodiscard]] const math::Segment& getRay(std::size_t index) const;

    // Returns the closest hit of the ray from the last cast, or nothing when it hit nothing.
    [[nodiscard]] const std::optional<RaycastHit>& getHit(std::size_t index) const;

  private:
    friend class Raycaster;

    void requireIndex(std::size_t index) const;

    std::vector<math::Segment> rays;
    std::vector<std::optional<RaycastHit>> results;
};

} // namespace haylen::physics2d
