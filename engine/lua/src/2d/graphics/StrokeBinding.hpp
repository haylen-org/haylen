#pragma once

#include <array>
#include <optional>
#include <string_view>
#include <vector>

#include "haylen/2d/graphics/Stroke.hpp"
#include "haylen/core/EnumNames.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"

struct lua_State;

namespace haylen::core {

template <> struct EnumNames<haylen::graphics2d::Stroke::Join> {
    static std::optional<haylen::graphics2d::Stroke::Join> fromName(std::string_view name) {
        return haylen::graphics2d::Stroke::joinFromName(name);
    }
    static std::string_view name(haylen::graphics2d::Stroke::Join value) {
        return haylen::graphics2d::Stroke::joinName(value);
    }
};

template <> struct EnumNames<haylen::graphics2d::Stroke::Cap> {
    static std::optional<haylen::graphics2d::Stroke::Cap> fromName(std::string_view name) {
        return haylen::graphics2d::Stroke::capFromName(name);
    }
    static std::string_view name(haylen::graphics2d::Stroke::Cap value) {
        return haylen::graphics2d::Stroke::capName(value);
    }
};

template <> struct EnumNames<haylen::graphics2d::Stroke::TextureMode> {
    static std::optional<haylen::graphics2d::Stroke::TextureMode> fromName(std::string_view name) {
        return haylen::graphics2d::Stroke::textureModeFromName(name);
    }
    static std::string_view name(haylen::graphics2d::Stroke::TextureMode value) {
        return haylen::graphics2d::Stroke::textureModeName(value);
    }
};

} // namespace haylen::core

namespace haylen::lua::graphics2d {

// The function `graphics2d.drawStroke(points, options)`, whose points, widths and dashes come from flat lists of numbers or float buffers.
class StrokeBinding final {
  public:
    static int drawStroke(lua_State* L);

    // Reads the numbers of a list or a float buffer at the index, and pairs of them as points.
    static void readNumbers(lua_State* L, int index, std::string_view what, std::vector<float>& values);
    static void readPoints(lua_State* L, int index, std::vector<haylen::math::Vec2>& target);

  private:
    static constexpr std::array<std::string_view, 14> kFields{"count", "width", "widths", "color", "colors", "join", "cap", "miterLimit", "closed", "texture", "textureMode", "textureLength", "dashes", "dashOffset"};

    // Kept from draw to draw on the frame thread, so a stroke drawn every frame allocates nothing once they have grown.
    inline static std::vector<haylen::math::Vec2> points;
    inline static std::vector<float> numbers;
    inline static std::vector<float> widths;
    inline static std::vector<haylen::math::Color> colors;
    inline static std::vector<float> dashes;
};

} // namespace haylen::lua::graphics2d
