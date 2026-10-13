#include "input/CommandBufferLua.hpp"

#include <lua.hpp>

#include <limits>
#include <string>
#include <utility>

#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/Userdata.hpp"
#include "input/ActionMapLua.hpp"

namespace haylen::input {

std::optional<std::uint32_t> CommandBufferLua::readLifetime(lua_State* L, int index) {
    if (lua_isboolean(L, index) && lua_toboolean(L, index) == 0) {
        return std::nullopt;
    }
    int isInteger = 0;
    const lua_Integer steps = lua_tointegerx(L, index, &isInteger);
    if (isInteger == 0 || steps < 1 || steps > static_cast<lua_Integer>(std::numeric_limits<std::uint32_t>::max())) {
        luaL_error(L, "A command lasts a whole number of steps from 1, or false to last until it is consumed.");
    }
    return static_cast<std::uint32_t>(steps);
}

// Resolves the button name at the index, or a direction when the buffer has directions.
std::size_t CommandBufferLua::readButton(lua_State* L, const CommandBuffer::Options& options, int index, const char* command) {
    const std::string_view name = lua::Stack::read<std::string_view>(L, index);
    if (options.directions && (name == "forward" || name == "back")) {
        return name == "forward" ? CommandBuffer::kForward : CommandBuffer::kBack;
    }
    for (std::size_t button = 0; button < options.buttons.size(); ++button) {
        if (options.buttons[button] == name) {
            return button;
        }
    }
    luaL_error(L, "The command \"%s\" names the button \"%s\", which the buffer does not have.", command, std::string(name).c_str());
    return 0;
}

// Creates a buffer with `newCommandBuffer({buttons, lifetime, lifetimes, directions, commands})`.
int CommandBufferLua::newCommandBuffer(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kOptions});
    CommandBuffer::Options options;
    lua::Table::readField(L, 1, "buttons", options.buttons);

    std::optional<std::uint32_t> lifetime = kDefaultLifetime;
    if (lua_getfield(L, 1, "lifetime") != LUA_TNIL) {
        lifetime = readLifetime(L, -1);
    }
    lua_pop(L, 1);
    options.lifetimes.assign(options.buttons.size(), lifetime);
    if (lua_getfield(L, 1, "lifetimes") != LUA_TNIL) {
        luaL_checktype(L, -1, LUA_TTABLE);
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            const std::string_view button = lua_type(L, -2) == LUA_TSTRING ? lua_tostring(L, -2) : "";
            std::size_t found = 0;
            while (found < options.buttons.size() && options.buttons[found] != button) {
                ++found;
            }
            if (found == options.buttons.size()) {
                return luaL_error(L, "The lifetimes name the button \"%s\", which the buffer does not have.", std::string(button).c_str());
            }
            options.lifetimes[found] = readLifetime(L, -1);
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    if (lua_getfield(L, 1, "directions") != LUA_TNIL) {
        luaL_checktype(L, -1, LUA_TTABLE);
        const int directions = lua_gettop(L);
        lua::Table::checkFields(L, directions, {kDirectionOptions});
        lua_getfield(L, directions, "back");
        lua_getfield(L, directions, "forward");
        if (lua_type(L, -1) != LUA_TSTRING || lua_type(L, -2) != LUA_TSTRING) {
            return luaL_error(L, "The directions of a command buffer name the \"back\" and the \"forward\" buttons.");
        }
        options.directions = std::array<std::size_t, 2>{readButton(L, options, -2, "directions"), readButton(L, options, -1, "directions")};
        lua_pop(L, 2);
    }
    lua_pop(L, 1);

    if (lua_getfield(L, 1, "commands") != LUA_TNIL) {
        luaL_checktype(L, -1, LUA_TTABLE);
        const int commands = lua_gettop(L);
        const auto count = static_cast<lua_Integer>(lua_rawlen(L, commands));
        for (lua_Integer entry = 1; entry <= count; ++entry) {
            lua_rawgeti(L, commands, entry);
            const int table = lua_gettop(L);
            luaL_checktype(L, table, LUA_TTABLE);
            lua::Table::checkFields(L, table, {kCommandOptions});
            CommandBuffer::Command command{.lifetime = lifetime};
            lua::Table::readField(L, table, "name", command.name);
            lua::Table::readField(L, table, "priority", command.priority);
            lua::Table::readField(L, table, "clearsHistory", command.clearsHistory);
            std::optional<std::uint32_t> maxTicks;
            lua::Table::readField(L, table, "maxTicks", maxTicks);
            command.maxTicks = maxTicks;
            if (lua_getfield(L, table, "lifetime") != LUA_TNIL) {
                command.lifetime = readLifetime(L, -1);
            }
            lua_pop(L, 1);
            if (lua_getfield(L, table, "steps") == LUA_TTABLE) {
                const auto steps = static_cast<lua_Integer>(lua_rawlen(L, -1));
                for (lua_Integer step = 1; step <= steps; ++step) {
                    lua_rawgeti(L, -1, step);
                    command.steps.push_back(readButton(L, options, -1, command.name.c_str()));
                    lua_pop(L, 1);
                }
            }
            lua_pop(L, 2);
            options.commands.push_back(std::move(command));
        }
    }
    lua_pop(L, 1);

    CommandBuffer& buffer = lua::Userdata::emplace<CommandBuffer>(L, std::move(options));
    const int owner = lua_gettop(L);
    lua_createtable(L, static_cast<int>(buffer.getCommandCount()), 0);
    for (std::size_t command = 0; command < buffer.getCommandCount(); ++command) {
        lua::Stack::push(L, buffer.getCommandName(command));
        lua_rawseti(L, -2, static_cast<lua_Integer>(command + 1));
    }
    lua::Userdata::setField(L, owner, "names", -1);
    lua_pop(L, 1);
    return 1;
}

template <void (CommandBuffer::*FromMask)(std::uint32_t) noexcept, void (CommandBuffer::*FromMap)(const ActionMap&) noexcept> int CommandBufferLua::feed(lua_State* L) {
    CommandBuffer& buffer = lua::Userdata::check<CommandBuffer>(L, 1);
    if (const ActionMap* map = lua::Userdata::test<ActionMap>(L, 2)) {
        (buffer.*FromMap)(*map);
        return 0;
    }
    const lua_Integer mask = luaL_checkinteger(L, 2);
    luaL_argcheck(L, mask >= 0 && mask <= static_cast<lua_Integer>(std::numeric_limits<std::uint32_t>::max()), 2, "the buttons of a step are an integer of 32 bits");
    (buffer.*FromMask)(static_cast<std::uint32_t>(mask));
    return 0;
}

std::size_t CommandBufferLua::checkCommand(lua_State* L, const CommandBuffer& buffer, int index) {
    const std::string_view name = lua::Stack::read<std::string_view>(L, index);
    const std::optional<std::size_t> command = buffer.findCommand(name);
    if (!command) {
        luaL_error(L, "The command buffer has no command \"%s\".", std::string(name).c_str());
    }
    return *command;
}

int CommandBufferLua::peek(lua_State* L) {
    const std::optional<std::size_t> kept = lua::Userdata::check<CommandBuffer>(L, 1).peek();
    if (!kept) {
        lua_pushnil(L);
        return 1;
    }
    lua::Userdata::pushField(L, 1, "names");
    lua_rawgeti(L, -1, static_cast<lua_Integer>(*kept + 1));
    return 1;
}

int CommandBufferLua::consume(lua_State* L) {
    CommandBuffer& buffer = lua::Userdata::check<CommandBuffer>(L, 1);
    lua::Stack::push(L, buffer.consume(checkCommand(L, buffer, 2)));
    return 1;
}

int CommandBufferLua::held(lua_State* L) {
    const CommandBuffer& buffer = lua::Userdata::check<CommandBuffer>(L, 1);
    const std::string_view name = lua::Stack::read<std::string_view>(L, 2);
    const std::optional<std::size_t> button = buffer.findButton(name);
    if (!button) {
        return luaL_error(L, "The command buffer has no button \"%s\".", std::string(name).c_str());
    }
    lua::Stack::push(L, buffer.isHeld(*button));
    return 1;
}

int CommandBufferLua::clear(lua_State* L) {
    lua::Userdata::check<CommandBuffer>(L, 1).clear();
    return 0;
}

int CommandBufferLua::frozen(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<CommandBuffer>(L, 1).isFrozen());
    return 1;
}

int CommandBufferLua::setFrozen(lua_State* L) {
    lua::Userdata::check<CommandBuffer>(L, 1).setFrozen(lua::Stack::read<bool>(L, 3));
    return 0;
}

int CommandBufferLua::facing(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<CommandBuffer>(L, 1).getFacing());
    return 1;
}

int CommandBufferLua::setFacing(lua_State* L) {
    lua::Userdata::check<CommandBuffer>(L, 1).setFacing(luaL_checknumber(L, 3) < 0.0 ? -1 : 1);
    return 0;
}

int CommandBufferLua::tick(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(lua::Userdata::check<CommandBuffer>(L, 1).getTick()));
    return 1;
}

void CommandBufferLua::install(lua_State* L) {
    lua::ClassBuilder<CommandBuffer>(L).function("update", &feed<&CommandBuffer::update, &CommandBuffer::update>).function("settle", &feed<&CommandBuffer::settle, &CommandBuffer::settle>).function("peek", &peek).function("consume", &consume).function("held", &held).function("clear", &clear).property("frozen", &frozen, &setFrozen).property("facing", &facing, &setFacing).property("tick", &tick).install();
}

void CommandBufferLua::addFunctions(lua_State* L) {
    lua_pushcfunction(L, &lua::Binding::native<&newCommandBuffer>);
    lua_setfield(L, -2, "newCommandBuffer");
}

} // namespace haylen::input
