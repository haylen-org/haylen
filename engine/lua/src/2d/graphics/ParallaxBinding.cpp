#include "2d/graphics/ParallaxBinding.hpp"

#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/Parallax.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua::graphics2d {

int ParallaxBinding::offset(lua_State* L) {
    const haylen::graphics2d::Parallax& layer = lua::Userdata::check<haylen::graphics2d::Parallax>(L, 1);
    const haylen::math::Vec2 shift = layer.getOffset(lua::Userdata::check<haylen::graphics2d::Camera>(L, 2));
    lua::Stack::push(L, shift.x);
    lua::Stack::push(L, shift.y);
    return 2;
}

int ParallaxBinding::draw(lua_State* L) {
    const haylen::graphics2d::Parallax& layer = lua::Userdata::check<haylen::graphics2d::Parallax>(L, 1);
    const haylen::graphics2d::Camera& camera = lua::Userdata::check<haylen::graphics2d::Camera>(L, 2);
    layer.draw(lua::Runtime::getEngine(L).getRenderer2D(), camera, lua::TypeConverter::readDrawOrder(L, 3));
    return 0;
}

void ParallaxBinding::install(lua_State* L) {
    lua::ClassBuilder<haylen::graphics2d::Parallax>(L).field<&haylen::graphics2d::Parallax::texture>("texture").field<&haylen::graphics2d::Parallax::source>("source").field<&haylen::graphics2d::Parallax::position>("position").field<&haylen::graphics2d::Parallax::size>("size").field<&haylen::graphics2d::Parallax::scrollScale>("scrollScale").field<&haylen::graphics2d::Parallax::repeatX>("repeatX").field<&haylen::graphics2d::Parallax::repeatY>("repeatY").field<&haylen::graphics2d::Parallax::repeatSize>("repeatSize").field<&haylen::graphics2d::Parallax::autoscroll>("autoscroll").field<&haylen::graphics2d::Parallax::limits>("limits").field<&haylen::graphics2d::Parallax::color>("color").method<&haylen::graphics2d::Parallax::getScrolled>("scrolled").method<&haylen::graphics2d::Parallax::update>("update").function("offset", &offset).function("draw", &lua::Binding::native<&draw>).install();
}

} // namespace haylen::lua::graphics2d
