#include "2d/graphics/ParallaxLua.hpp"

#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/Parallax.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::graphics2d {

int ParallaxLua::offset(lua_State* L) {
    const math::Vec2 shift = lua::Userdata::check<Parallax>(L, 1).getOffset(lua::Userdata::check<Camera>(L, 2));
    lua::Stack::push(L, shift.x);
    lua::Stack::push(L, shift.y);
    return 2;
}

int ParallaxLua::draw(lua_State* L) {
    core::Engine& engine = lua::Runtime::getEngine(L);
    lua::Userdata::check<Parallax>(L, 1).draw(engine.getRenderer2D(), lua::Userdata::check<Camera>(L, 2), engine.getViewport().getVisibleRect(), lua::TypeConverter::readDrawOrder(L, 3));
    return 0;
}

void ParallaxLua::install(lua_State* L) {
    lua::ClassBuilder<Parallax>(L).field<&Parallax::texture>("texture").field<&Parallax::source>("source").field<&Parallax::position>("position").field<&Parallax::size>("size").field<&Parallax::scrollScale>("scrollScale").field<&Parallax::repeatX>("repeatX").field<&Parallax::repeatY>("repeatY").field<&Parallax::repeatSize>("repeatSize").field<&Parallax::autoscroll>("autoscroll").field<&Parallax::limits>("limits").field<&Parallax::color>("color").method<&Parallax::getScrolled>("scrolled").method<&Parallax::update>("update").function("offset", &offset).function("draw", &lua::Binding::native<&draw>).install();
}

} // namespace haylen::graphics2d
