#pragma once

#include <array>
#include <optional>
#include <string_view>
#include <vector>

#include "haylen/2d/graphics/Stroke.hpp"
#include "haylen/lua/EnumNames.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"

struct lua_State;

namespace haylen::lua {

template <> struct EnumNames<graphics2d::Stroke::Join> {
    static std::optional<graphics2d::Stroke::Join> fromName(std::string_view name) {
        return graphics2d::Stroke::joinFromName(name);
    }
    static std::string_view name(graphics2d::Stroke::Join value) {
        return graphics2d::Stroke::joinName(value);
    }
};

template <> struct EnumNames<graphics2d::Stroke::Cap> {
    static std::optional<graphics2d::Stroke::Cap> fromName(std::string_view name) {
        return graphics2d::Stroke::capFromName(name);
    }
    static std::string_view name(graphics2d::Stroke::Cap value) {
        return graphics2d::Stroke::capName(value);
    }
};

template <> struct EnumNames<graphics2d::Stroke::TextureMode> {
    static std::optional<graphics2d::Stroke::TextureMode> fromName(std::string_view name) {
        return graphics2d::Stroke::textureModeFromName(name);
    }
    static std::string_view name(graphics2d::Stroke::TextureMode value) {
        return graphics2d::Stroke::textureModeName(value);
    }
};

} // namespace haylen::lua

namespace haylen::graphics2d {

// The function `graphics2d.drawStroke(points, options)`, whose points, widths and dashes come from flat lists of numbers or float buffers.
class StrokeLua final {
  public:
    static int drawStroke(lua_State* L);

    // Reads the numbers of a list or a float buffer at the index, and pairs of them as points.
    static void readNumbers(lua_State* L, int index, std::string_view what, std::vector<float>& values);
    static void readPoints(lua_State* L, int index, std::vector<math::Vec2>& target);

  private:
    static constexpr std::array<std::string_view, 14> kFields{"count", "width", "widths", "color", "colors", "join", "cap", "miterLimit", "closed", "texture", "textureMode", "textureLength", "dashes", "dashOffset"};

    // Kept from draw to draw on the frame thread, so a stroke drawn every frame allocates nothing once they have grown.
    inline static std::vector<math::Vec2> points;
    inline static std::vector<float> numbers;
    inline static std::vector<float> widths;
    inline static std::vector<math::Color> colors;
    inline static std::vector<float> dashes;
};

} // namespace haylen::graphics2d
