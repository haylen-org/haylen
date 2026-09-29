#include "2d/spatial/Spatial2DLua.hpp"

#include <lua.hpp>

#include "2d/spatial/GridLua.hpp"
#include "2d/spatial/SpatialIndexLua.hpp"
#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/spatial/ScreenPicker.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::spatial2d {

// Returns the ray from the center of the view to the world point under a screen point with screenRay(camera, x, y), as x1, y1, x2, y2.
int Spatial2DLua::screenRay(lua_State* L) {
    const ScreenPicker picker(lua::Userdata::check<graphics2d::Camera>(L, 1), lua::Runtime::getEngine(L).getViewport().getVisibleRect());
    const math::Ray ray = picker.toRay({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)});
    const math::Vec2 end = ray.getEnd();
    lua::Stack::push(L, ray.origin.x);
    lua::Stack::push(L, ray.origin.y);
    lua::Stack::push(L, end.x);
    lua::Stack::push(L, end.y);
    return 4;
}

int Spatial2DLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"screenRay", &lua::Binding::native<&screenRay>},
        {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    SpatialIndexLua::addFunctions(L);
    GridLua::addFunctions(L);
    return 1;
}

void Spatial2DLua::install(lua_State* L) {
    SpatialIndexLua::install(L);
    GridLua::install(L);
    lua::Binding::preload(L, "haylen.spatial2d", &open);
}

} // namespace haylen::spatial2d
