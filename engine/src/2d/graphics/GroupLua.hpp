#pragma once

#include <array>
#include <optional>
#include <string_view>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/lua/EnumNames.hpp"

struct lua_State;

namespace haylen::lua {

template <> struct EnumNames<graphics2d::Renderer::MaskMode> {
    static std::optional<graphics2d::Renderer::MaskMode> fromName(std::string_view name) {
        return graphics2d::Renderer::maskModeFromName(name);
    }
    static std::string_view name(graphics2d::Renderer::MaskMode value) {
        return graphics2d::Renderer::maskModeName(value);
    }
};

} // namespace haylen::lua

namespace haylen::graphics2d {

// The functions of `haylen.graphics2d` that draw groups as one image and masks of any shape.
class GroupLua final {
  public:
    static int beginGroup(lua_State* L);
    static int endGroup(lua_State* L);

    // Runs `pushMask(drawShape, options)` as three steps, so the function that draws the shape runs between them as a plain Lua call: the mask begins, the function draws its shape, and its content begins.
    static int pushMask(lua_State* L);
    static int popMask(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 1> kGroupFields{"opacity"};
    static constexpr std::array<std::string_view, 1> kMaskFields{"mode"};

    static int beginMask(lua_State* L);
    static int endMaskShape(lua_State* L);
};

} // namespace haylen::graphics2d
