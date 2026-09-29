#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "haylen/2d/particles/EmitterConfig.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/FloatRange.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::particles2d {

// A particle effect file holds the options of particles.newEmitter as JSON, with the texture named by its path relative to the effect file. Colors use #AARRGGBB and ranges take a number or [min, max].
struct Effect {
    std::string texturePath;
    EmitterConfig config;

    [[nodiscard]] static Effect parse(const core::Json& document, std::string_view path);

  private:
    [[nodiscard]] static math::FloatRange readRange(const core::Json& value);
    [[nodiscard]] static math::Vec2 readVec2(const core::Json& value);
    [[nodiscard]] static std::size_t readCount(const core::Json& value, std::string_view name);
    [[nodiscard]] static math::Color readColor(const core::Json& value);
};

} // namespace haylen::particles2d
