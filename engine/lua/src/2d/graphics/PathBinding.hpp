#pragma once

#include <array>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "haylen/2d/graphics/Path.hpp"
#include "haylen/core/EnumNames.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/math/Vec2.hpp"

struct lua_State;

namespace haylen::lua {

template <> struct Type<haylen::graphics2d::Path> {
    static constexpr const char* name = "haylen.Path";
    using Storage = std::shared_ptr<haylen::graphics2d::Path>;
};

} // namespace haylen::lua

namespace haylen::core {

template <> struct EnumNames<haylen::graphics2d::Path::FillRule> {
    static std::optional<haylen::graphics2d::Path::FillRule> fromName(std::string_view name) {
        return haylen::graphics2d::Path::fillRuleFromName(name);
    }
    static std::string_view name(haylen::graphics2d::Path::FillRule value) {
        return haylen::graphics2d::Path::fillRuleName(value);
    }
};

} // namespace haylen::core

namespace haylen::lua::graphics2d {

// Installs the `Path` class of `haylen.graphics2d`, whose commands return the path so they chain, and which fills and strokes itself at a place with a rotation and a scale.
class PathBinding final {
  public:
    static void install(lua_State* L);

    // Creates an empty path with `graphics2d.newPath()`.
    static int create(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 6> kFillFields{"color", "fillRule", "texture", "rotation", "scaleX", "scaleY"};
    static constexpr std::array<std::string_view, 13> kStrokeFields{"width", "color", "join", "cap", "miterLimit", "texture", "textureMode", "textureLength", "dashes", "dashOffset", "rotation", "scaleX", "scaleY"};

    // Reads the x and y arguments at the index and the one after it.
    [[nodiscard]] static haylen::math::Vec2 checkPoint(lua_State* L, int index);

    static int moveTo(lua_State* L);
    static int lineTo(lua_State* L);
    static int quadraticTo(lua_State* L);
    static int cubicTo(lua_State* L);
    static int arc(lua_State* L);
    static int close(lua_State* L);
    static int clear(lua_State* L);
    static int fill(lua_State* L);
    static int stroke(lua_State* L);
    static int outlines(lua_State* L);
    static int isEmpty(lua_State* L);

    // Kept from draw to draw on the frame thread, so a stroke drawn every frame allocates nothing once they have grown.
    inline static std::vector<float> dashes;
};

} // namespace haylen::lua::graphics2d
