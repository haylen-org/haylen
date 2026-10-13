#pragma once

#include <array>
#include <memory>
#include <string_view>

#include "haylen/2d/graphics/PixelCanvas.hpp"
#include "haylen/lua/Type.hpp"

struct lua_State;

namespace haylen::lua {

template <> struct Type<graphics2d::PixelCanvas> {
    static constexpr const char* name = "haylen.PixelCanvas";
    using Storage = std::shared_ptr<graphics2d::PixelCanvas>;
};

} // namespace haylen::lua

namespace haylen::graphics2d {

// Installs the `PixelCanvas` class of `haylen.graphics2d`.
class PixelCanvasLua final {
  public:
    static void install(lua_State* L);

    // Creates a canvas with `graphics2d.newPixelCanvas(width, height, {smooth})`.
    static int create(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 1> kOptionFields{"smooth"};

    static int begin(lua_State* L);
    static int draw(lua_State* L);
    static int fraction(lua_State* L);
    static int width(lua_State* L);
    static int height(lua_State* L);
    static int texture(lua_State* L);
};

} // namespace haylen::graphics2d
