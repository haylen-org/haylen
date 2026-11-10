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

    // Runs `pushMask(drawShape, options)` as three steps, so the function that draws the shape runs between them as a plain Lua call: the mask begins, the function draws its shape, and its content begins. A shape function that fails ends the mask it began, even when a protected call of the app catches the error.
    static int pushMask(lua_State* L);
    static int popMask(lua_State* L);

  private:
    // The state of one running `pushMask`, which a to-be-closed value on its stack holds. The closers stay in a table of the registry for the next calls, one for each level of masks that nest while their shapes draw, next to their metatable, which no script reaches.
    struct MaskCloser {
        bool busy = false;
        bool shapeOpen = false;
    };

    static constexpr std::array<std::string_view, 1> kGroupFields{"opacity"};
    static constexpr std::array<std::string_view, 1> kMaskFields{"mode"};
    static constexpr const char* kClosersKey = "haylen.maskClosers";

    static int beginMask(lua_State* L);
    static int endMaskShape(lua_State* L);
    [[nodiscard]] static MaskCloser& pushCloser(lua_State* L);
    static int closeMask(lua_State* L);
};

} // namespace haylen::graphics2d
