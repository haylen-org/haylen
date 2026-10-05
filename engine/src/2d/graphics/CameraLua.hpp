#pragma once

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

struct lua_State;

namespace haylen::graphics2d {

// Installs the `Camera` class of `haylen.graphics2d`. Its methods measure the view with the visible design area as the screen, as world canvases do, and take screen points in design coordinates.
class CameraLua final {
  public:
    static void install(lua_State* L);

  private:
    [[nodiscard]] static math::Rect getScreen(lua_State* L);
    [[nodiscard]] static math::Vec2 readPoint(lua_State* L, int index);
    static int pushPoint(lua_State* L, math::Vec2 point);

    static int follow(lua_State* L);
    static int frame(lua_State* L);
    static int clampToLimits(lua_State* L);
    static int resetSmoothing(lua_State* L);
    static int align(lua_State* L);
    static int snapTo(lua_State* L);
    static int zoomAt(lua_State* L);
    static int shake(lua_State* L);
    static int flash(lua_State* L);
    static int visibleBounds(lua_State* L);
    static int viewTransform(lua_State* L);
    static int viewSize(lua_State* L);
    static int worldToScreen(lua_State* L);
    static int screenToWorld(lua_State* L);
    static int drawDebug(lua_State* L);
    static int getX(lua_State* L);
    static int setX(lua_State* L);
    static int getY(lua_State* L);
    static int setY(lua_State* L);
    static int getDragMargins(lua_State* L);
    static int setDragMargins(lua_State* L);
};

} // namespace haylen::graphics2d
