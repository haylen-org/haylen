#include "platform/OutputLua.hpp"

#include <lua.hpp>

#include <algorithm>
#include <string>
#include <utility>

#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/platform/Outputs.hpp"

namespace haylen::platform {

// Opens an output with `display.open(id, {fullscreen, width, height, title, clearColor})`, where the size of a window on a desktop is in points.
int OutputLua::open(lua_State* L) {
    const auto display = lua::Stack::read<std::string_view>(L, 1);
    Output::Options options;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kOptions});
        lua::Table::readField(L, 2, "fullscreen", options.fullscreen);
        lua::Table::readField(L, 2, "width", options.size.x);
        lua::Table::readField(L, 2, "height", options.size.y);
        lua::Table::readField(L, 2, "title", options.title);
        lua::Table::readField(L, 2, "clearColor", options.clearColor);
    }
    if (!(options.size.x > 0.0F && options.size.y > 0.0F)) {
        luaL_error(L, "The window of an output needs a positive \"width\" and \"height\" in points.");
    }
    lua::Userdata::emplace<Output>(L, lua::Runtime::getEngine(L).getOutputs().open(display, options));
    return 1;
}

int OutputLua::id(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(lua::Userdata::check<Output>(L, 1).getId()));
    return 1;
}

int OutputLua::display(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Output>(L, 1).getDisplayId());
    return 1;
}

int OutputLua::state(lua_State* L) {
    const Output::State value = lua::Userdata::check<Output>(L, 1).getState();
    lua::Stack::push(L, std::ranges::find(Output::kStateNames, value, &std::pair<std::string_view, Output::State>::second)->first);
    return 1;
}

int OutputLua::shown(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Output>(L, 1).getState() == Output::State::Shown);
    return 1;
}

// The ending matters once the output closed, and an output that is open has none.
int OutputLua::ending(lua_State* L) {
    const Output& output = lua::Userdata::check<Output>(L, 1);
    if (output.getState() != Output::State::Closed) {
        lua_pushnil(L);
        return 1;
    }
    lua::Stack::push(L, std::ranges::find(Output::kEndingNames, output.getEnding(), &std::pair<std::string_view, Output::Ending>::second)->first);
    return 1;
}

int OutputLua::width(lua_State* L) {
    const graphics::RenderTarget& value = lua::Userdata::check<Output>(L, 1).getTarget();
    lua::Stack::push(L, value.isValid() ? value.getWidth() : 0);
    return 1;
}

int OutputLua::height(lua_State* L) {
    const graphics::RenderTarget& value = lua::Userdata::check<Output>(L, 1).getTarget();
    lua::Stack::push(L, value.isValid() ? value.getHeight() : 0);
    return 1;
}

int OutputLua::scale(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Output>(L, 1).getScale());
    return 1;
}

// The target is `nil` until the output first shows.
int OutputLua::target(lua_State* L) {
    const graphics::RenderTarget& value = lua::Userdata::check<Output>(L, 1).getTarget();
    if (!value.isValid()) {
        lua_pushnil(L);
        return 1;
    }
    lua::Stack::push(L, value);
    return 1;
}

int OutputLua::clearColor(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Output>(L, 1).getClearColor());
    return 1;
}

int OutputLua::setClearColor(lua_State* L) {
    lua::Userdata::check<Output>(L, 1).setClearColor(lua::Stack::read<math::Color>(L, 3));
    return 0;
}

int OutputLua::close(lua_State* L) {
    lua::Runtime::getEngine(L).getOutputs().close(lua::Userdata::check<Output>(L, 1));
    return 0;
}

void OutputLua::install(lua_State* L) {
    lua::ClassBuilder<Output>(L).property("id", &id).property("display", &display).property("state", &state).property("shown", &shown).property("ending", &ending).property("width", &width).property("height", &height).property("scale", &scale).property("target", &target).property("clearColor", &clearColor, &lua::Binding::native<&setClearColor>).function("close", &lua::Binding::native<&close>).meta("__eq", &lua::Userdata::equal<Output>).install();
}

} // namespace haylen::platform
