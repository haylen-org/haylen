#include "2d/physics/ForceFieldLua.hpp"

#include <lua.hpp>

#include <optional>
#include <string_view>
#include <vector>

#include "2d/physics/Physics2DLua.hpp"
#include "2d/physics/ScriptedOwner.hpp"
#include "2d/physics/ShapeLua.hpp"
#include "haylen/2d/physics/ForceField.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"

namespace haylen::lua {

template <> struct Type<physics2d::ScriptedOwner<physics2d::ForceField>> {
    static constexpr const char* name = "haylen.ForceField";
    using Storage = physics2d::ScriptedOwner<physics2d::ForceField>;
};

template <> struct EnumNames<physics2d::ForceField::Kind> {
    static std::optional<physics2d::ForceField::Kind> fromName(std::string_view name) {
        return physics2d::ForceField::kindFromName(name);
    }
    static std::string_view name(physics2d::ForceField::Kind value) {
        return physics2d::ForceField::kindName(value);
    }
};

template <> struct EnumNames<physics2d::ForceField::Falloff> {
    static std::optional<physics2d::ForceField::Falloff> fromName(std::string_view name) {
        return physics2d::ForceField::falloffFromName(name);
    }
    static std::string_view name(physics2d::ForceField::Falloff value) {
        return physics2d::ForceField::falloffName(value);
    }
};

} // namespace haylen::lua

namespace haylen::physics2d {

ForceField& ForceFieldLua::check(lua_State* L) {
    ForceField& field = lua::Userdata::check<ScriptedOwner<ForceField>>(L, 1).object;
    if (!field.isValid()) {
        luaL_error(L, "The force field was destroyed.");
    }
    return field;
}

// Creates a field with `newForceField(world, {kind, x, y, radius, width, height, points, strength, direction, falloff, minDistance, acceleration, density, linearDrag, angularDrag, flow, category, mask, group, enabled})`. Its user value is the world object.
int ForceFieldLua::newForceField(lua_State* L) {
    const std::shared_ptr<World>& world = lua::Userdata::checkShared<World>(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    lua::Table::checkFields(L, 2, {kFieldFields});
    ForceField::Options options;
    lua::Table::readField(L, 2, "kind", options.kind);
    lua::Table::readField(L, 2, "x", options.position.x);
    lua::Table::readField(L, 2, "y", options.position.y);
    lua::Table::readField(L, 2, "radius", options.radius);
    lua::Table::readField(L, 2, "width", options.size.x);
    lua::Table::readField(L, 2, "height", options.size.y);
    lua::Table::readField(L, 2, "points", options.points);
    lua::Table::readField(L, 2, "strength", options.strength);
    lua::Table::readField(L, 2, "direction", options.direction);
    lua::Table::readField(L, 2, "falloff", options.falloff);
    lua::Table::readField(L, 2, "minDistance", options.minDistance);
    lua::Table::readField(L, 2, "acceleration", options.acceleration);
    lua::Table::readField(L, 2, "density", options.density);
    lua::Table::readField(L, 2, "linearDrag", options.linearDrag);
    lua::Table::readField(L, 2, "angularDrag", options.angularDrag);
    lua::Table::readField(L, 2, "flow", options.flow);
    lua::Table::readField(L, 2, "enabled", options.enabled);
    options.filter = ShapeLua::readFilter(L, 2, {});
    lua::Userdata::emplace<ScriptedOwner<ForceField>>(L, world, options);
    lua_pushvalue(L, 1);
    lua_setiuservalue(L, -2, 1);
    return 1;
}

int ForceFieldLua::destroy(lua_State* L) {
    lua::Userdata::check<ScriptedOwner<ForceField>>(L, 1).object.destroy();
    return 0;
}

int ForceFieldLua::isValid(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedOwner<ForceField>>(L, 1).object.isValid());
    return 1;
}

int ForceFieldLua::getKind(lua_State* L) {
    lua::Stack::push(L, check(L).getOptions().kind);
    return 1;
}

int ForceFieldLua::isEnabled(lua_State* L) {
    lua::Stack::push(L, check(L).getOptions().enabled);
    return 1;
}

int ForceFieldLua::setEnabled(lua_State* L) {
    check(L).setEnabled(lua::Stack::read<bool>(L, 3));
    return 0;
}

int ForceFieldLua::getStrength(lua_State* L) {
    lua::Stack::push(L, check(L).getOptions().strength);
    return 1;
}

int ForceFieldLua::setStrength(lua_State* L) {
    check(L).setStrength(lua::Stack::read<float>(L, 3));
    return 0;
}

int ForceFieldLua::getPosition(lua_State* L) {
    lua::Stack::push(L, check(L).getOptions().position);
    return 1;
}

int ForceFieldLua::setPosition(lua_State* L) {
    check(L).setPosition(lua::Stack::read<math::Vec2>(L, 3));
    return 0;
}

int ForceFieldLua::getDirection(lua_State* L) {
    lua::Stack::push(L, check(L).getOptions().direction);
    return 1;
}

int ForceFieldLua::setDirection(lua_State* L) {
    check(L).setDirection(lua::Stack::read<math::Vec2>(L, 3));
    return 0;
}

int ForceFieldLua::getFlow(lua_State* L) {
    lua::Stack::push(L, check(L).getOptions().flow);
    return 1;
}

int ForceFieldLua::setFlow(lua_State* L) {
    check(L).setFlow(lua::Stack::read<math::Vec2>(L, 3));
    return 0;
}

int ForceFieldLua::getDensity(lua_State* L) {
    lua::Stack::push(L, check(L).getOptions().density);
    return 1;
}

int ForceFieldLua::setDensity(lua_State* L) {
    check(L).setDensity(lua::Stack::read<float>(L, 3));
    return 0;
}

int ForceFieldLua::getLinearDrag(lua_State* L) {
    lua::Stack::push(L, check(L).getOptions().linearDrag);
    return 1;
}

int ForceFieldLua::setLinearDrag(lua_State* L) {
    check(L).setLinearDrag(lua::Stack::read<float>(L, 3));
    return 0;
}

int ForceFieldLua::getAngularDrag(lua_State* L) {
    lua::Stack::push(L, check(L).getOptions().angularDrag);
    return 1;
}

int ForceFieldLua::setAngularDrag(lua_State* L) {
    check(L).setAngularDrag(lua::Stack::read<float>(L, 3));
    return 0;
}

int ForceFieldLua::getBounds(lua_State* L) {
    lua::Stack::push(L, check(L).getBounds());
    return 1;
}

int ForceFieldLua::getBodyCount(lua_State* L) {
    lua::Stack::push(L, check(L).getBodyCount());
    return 1;
}

void ForceFieldLua::install(lua_State* L) {
    lua::ClassBuilder<ScriptedOwner<ForceField>>(L).function("destroy", &lua::Binding::native<&destroy>).property("valid", &lua::Binding::native<&isValid>).property("kind", &lua::Binding::native<&getKind>).property("enabled", &lua::Binding::native<&isEnabled>, &lua::Binding::native<&setEnabled>).property("strength", &lua::Binding::native<&getStrength>, &lua::Binding::native<&setStrength>).property("position", &lua::Binding::native<&getPosition>, &lua::Binding::native<&setPosition>).property("direction", &lua::Binding::native<&getDirection>, &lua::Binding::native<&setDirection>).property("flow", &lua::Binding::native<&getFlow>, &lua::Binding::native<&setFlow>).property("density", &lua::Binding::native<&getDensity>, &lua::Binding::native<&setDensity>).property("linearDrag", &lua::Binding::native<&getLinearDrag>, &lua::Binding::native<&setLinearDrag>).property("angularDrag", &lua::Binding::native<&getAngularDrag>, &lua::Binding::native<&setAngularDrag>).property("bounds", &lua::Binding::native<&getBounds>).property("bodyCount", &lua::Binding::native<&getBodyCount>).install();
}

void ForceFieldLua::addFunctions(lua_State* L) {
    lua_pushcfunction(L, &lua::Binding::native<&newForceField>);
    lua_setfield(L, -2, "newForceField");
}

} // namespace haylen::physics2d
