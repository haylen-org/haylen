#pragma once

#include <cstdint>

namespace haylen::physics3d {

// Who a body collides with. Two bodies collide when the category of each one shares a bit with the mask of the other, so up to 32 categories exist. The group decides before both: bodies that share a positive group always collide, and bodies that share a negative group never do, such as the parts of one ragdoll.
struct CollisionFilter {
    std::uint32_t category = 1;
    std::uint32_t mask = ~std::uint32_t{0};
    int group = 0;

    [[nodiscard]] bool canCollide(const CollisionFilter& other) const noexcept {
        if (group != 0 && group == other.group) {
            return group > 0;
        }

        return (category & other.mask) != 0 && (other.category & mask) != 0;
    }

    // Tells whether a query with this filter finds a body with the other one, by the category of the body and the mask of the query alone.
    [[nodiscard]] bool canFind(const CollisionFilter& other) const noexcept {
        return (other.category & mask) != 0;
    }

    [[nodiscard]] friend bool operator==(const CollisionFilter&, const CollisionFilter&) noexcept = default;
};

} // namespace haylen::physics3d
