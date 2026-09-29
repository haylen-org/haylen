#include "2d/physics/JointLua.hpp"

#include "2d/physics/Physics2DLua.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"

namespace haylen::physics2d {

int JointLua::isValid(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedHandle<Joint>>(L, 1).handle.isValid());
    return 1;
}

int JointLua::destroy(lua_State* L) {
    lua::Userdata::check<ScriptedHandle<Joint>>(L, 1).handle.destroy();
    return 0;
}

int JointLua::setTarget(lua_State* L) {
    lua::Userdata::check<ScriptedHandle<Joint>>(L, 1).handle.setTarget({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)});
    return 0;
}

int JointLua::setMotorSpeed(lua_State* L) {
    lua::Userdata::check<ScriptedHandle<Joint>>(L, 1).handle.setMotorSpeed(lua::Stack::read<float>(L, 2));
    return 0;
}

void JointLua::install(lua_State* L) {
    lua::ClassBuilder<ScriptedHandle<Joint>>(L).function("destroy", &destroy).function("setTarget", &lua::Binding::native<&setTarget>).function("setMotorSpeed", &lua::Binding::native<&setMotorSpeed>).property("valid", &isValid).install();
}

} // namespace haylen::physics2d
