#pragma once

#include <optional>
#include <stdexcept>

#include "haylen/2d/physics/RaycastHit.hpp"

namespace haylen::test {

// Reads the results of the physics queries that a test expects to find something, and throws when one found nothing, so a test never reads an empty result.
class Physics2DHelper final {
  public:
    [[nodiscard]] static physics2d::RaycastHit requireHit(const std::optional<physics2d::RaycastHit>& hit) {
        if (!hit) {
            throw std::runtime_error("The cast hit nothing.");
        }

        return *hit;
    }
};

} // namespace haylen::test
