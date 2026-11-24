#include "2d/spatial/Spatial2DBinding.hpp"

#include <lua.hpp>

#include "2d/spatial/GridBinding.hpp"
#include "2d/spatial/OverlapWorldBinding.hpp"
#include "2d/spatial/SpatialIndexBinding.hpp"
#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/spatial/ScreenPicker.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua::spatial2d {

// Returns the ray from the center of the view to the world point under a screen point with `screenRay(camera, x, y)`, as `x1, y1, x2, y2`.
int Spatial2DBinding::screenRay(lua_State* L) {
    const haylen::spatial2d::ScreenPicker picker(lua::Userdata::check<haylen::graphics2d::Camera>(L, 1), lua::Runtime::getEngine(L).getViewport().getVisibleRect());
    const haylen::math::Ray2 ray = picker.toRay({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)});
    const haylen::math::Vec2 end = ray.getEnd();
    lua::Stack::push(L, ray.origin.x);
    lua::Stack::push(L, ray.origin.y);
    lua::Stack::push(L, end.x);
    lua::Stack::push(L, end.y);
    return 4;
}

int Spatial2DBinding::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"screenRay", &lua::Binding::native<&screenRay>},
        {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    SpatialIndexBinding::addFunctions(L);
    GridBinding::addFunctions(L);
    OverlapWorldBinding::addFunctions(L);
    return 1;
}

void Spatial2DBinding::install(lua_State* L) {
    SpatialIndexBinding::install(L);
    GridBinding::install(L);
    OverlapWorldBinding::install(L);
    lua::Binding::preload(L, "haylen.spatial2d", &open);
}

} // namespace haylen::lua::spatial2d
