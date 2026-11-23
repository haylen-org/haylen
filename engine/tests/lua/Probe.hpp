#pragma once

#include <array>
#include <memory>

#include "haylen/lua/Type.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::lua {

// A bound type of the tests with fields of the kinds that `ClassBuilder` records as native properties and of kinds it does not, an accessor pair and integer keys.
struct Probe {
    struct Order {
        float depth = 0.0F;
        int layer = 0;
    };

    [[nodiscard]] float getScale() const noexcept {
        return scale;
    }
    void setScale(float value) noexcept {
        scale = value;
    }

    float speed = 0.0F;
    haylen::math::Vec2 position;
    haylen::math::Color tint;
    Order order;
    bool visible = true;
    float scale = 1.0F;
    std::array<float, 2> values{};
};

template <> struct Type<Probe> {
    static constexpr const char* name = "haylen.test.Probe";
    using Storage = std::shared_ptr<Probe>;
};

} // namespace haylen::lua
