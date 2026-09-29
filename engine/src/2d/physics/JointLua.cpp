#include "2d/physics/JointLua.hpp"

#include "2d/physics/Physics2DLua.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"

namespace haylen::physics2d {

int JointLua::isValid(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedHandle<Joint>>(L, 1).handle.isValid());
    return 1;
}

int JointLua::destroy(lua_State* L) {
    lua::Userdata::check<ScriptedHandle<Joint>>(L, 1).handle.destroy();
    return 0;
}

int JointLua::getTarget(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedHandle<Joint>>(L, 1).handle.getTarget());
    return 1;
}

int JointLua::setTarget(lua_State* L) {
    lua::Userdata::check<ScriptedHandle<Joint>>(L, 1).handle.setTarget(lua::Stack::read<math::Vec2>(L, 3));
    return 0;
}

int JointLua::getMotorSpeed(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedHandle<Joint>>(L, 1).handle.getMotorSpeed());
    return 1;
}

int JointLua::setMotorSpeed(lua_State* L) {
    lua::Userdata::check<ScriptedHandle<Joint>>(L, 1).handle.setMotorSpeed(lua::Stack::read<float>(L, 3));
    return 0;
}

void JointLua::install(lua_State* L) {
    lua::ClassBuilder<ScriptedHandle<Joint>>(L).function("destroy", &destroy).property("valid", &isValid).property("target", &lua::Binding::native<&getTarget>, &lua::Binding::native<&setTarget>).property("motorSpeed", &lua::Binding::native<&getMotorSpeed>, &lua::Binding::native<&setMotorSpeed>).install();
}

} // namespace haylen::physics2d
