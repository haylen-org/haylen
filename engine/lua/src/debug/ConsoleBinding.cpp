#include "debug/ConsoleBinding.hpp"

#include <lua.hpp>

#include <algorithm>
#include <format>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/plugins/DebugPlugin.hpp"
#include "runtime/Owners.hpp"

namespace haylen::lua::debug {

haylen::debug::Console& ConsoleBinding::getConsole(lua_State* L) {
    return lua::Runtime::getEngine(L).getPlugin<haylen::plugins::DebugPlugin>().getConsole();
}

// Reads a kind such as `'integer'`, or `'integer?'` for an optional argument.
haylen::debug::Console::Parameter ConsoleBinding::readParameter(lua_State* L, int index) {
    std::string_view name = lua::Stack::read<std::string_view>(L, index);
    const bool optional = name.ends_with('?');
    if (optional) {
        name.remove_suffix(1);
    }

    const auto found = std::ranges::find(haylen::debug::Console::kKindNames, name, &std::pair<std::string_view, haylen::debug::Console::Kind>::first);
    if (found == haylen::debug::Console::kKindNames.end()) {
        throw std::invalid_argument(std::format("The argument kind \"{}\" is unknown. It is \"integer\", \"number\", \"string\", \"boolean\" or \"text\", with \"?\" after it for an optional argument.", name));
    }

    return {.kind = found->second, .optional = optional};
}

// Adds a command with `addCommand(name, fn, {help, args, owner})`, where `fn` receives the converted arguments and returns the text to print, and returns its connection. An owner holds the function and removes the command when it ends.
int ConsoleBinding::addCommand(lua_State* L) {
    haylen::debug::Console::Command command{.name = lua::Stack::read<std::string>(L, 1), .help = {}, .parameters = {}, .handler = {}};
    luaL_checktype(L, 2, LUA_TFUNCTION);
    int owner = 0;
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua::Table::checkFields(L, 3, {kCommandFields});
        if (lua_getfield(L, 3, "help") != LUA_TNIL) {
            command.help = lua::Stack::read<std::string>(L, -1);
        }

        if (lua_getfield(L, 3, "args") != LUA_TNIL) {
            lua::Table::requireType(L, "args", LUA_TTABLE);
            const int args = lua_gettop(L);
            for (lua_Integer index = 1; lua_rawgeti(L, args, index) != LUA_TNIL; ++index) {
                command.parameters.push_back(readParameter(L, -1));
                lua_pop(L, 1);
            }

            lua_pop(L, 1);
        }

        if (lua_getfield(L, 3, "owner") != LUA_TNIL) {
            lua::Owners::checkOwner(L, -1);
            owner = lua_gettop(L);
        }
    }

    if (owner == 0) {
        owner = lua::Owners::pushDefault(L);
    }

    auto function = std::make_shared<lua::Owners::Function>(L, 2, owner);
    lua_State* main = lua::Runtime::getMainThread(L);
    // clang-format off
    command.handler = [function, main](const haylen::core::Json& arguments) {
        if (!function->push(main)) {
            throw std::runtime_error("The command ended with its owner.");
        }

        for (const haylen::core::Json& argument : arguments) {
            lua::JsonConverter::push(main, argument);
        }

        lua::Runtime::protectedCall(main, static_cast<int>(arguments.size()), 1);
        const int type = lua_type(main, -1);
        std::string text = type == LUA_TSTRING || type == LUA_TNUMBER ? lua_tostring(main, -1) : "";
        lua_pop(main, 1);

        if (type != LUA_TNIL && type != LUA_TSTRING && type != LUA_TNUMBER) {
            throw std::runtime_error("A console command returns a string, a number or nothing.");
        }

        return text;
    };
    // clang-format on

    haylen::core::Connection connection = getConsole(L).addCommand(std::move(command));
    if (owner != 0) {
        lua::Owners::add(L, owner, connection);
    }

    lua::Userdata::emplace<haylen::core::Connection>(L, std::move(connection));
    return 1;
}

int ConsoleBinding::removeCommand(lua_State* L) {
    lua::Stack::push(L, getConsole(L).removeCommand(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

// Runs a line with `runCommand(line)` and returns whether it worked and what it printed.
int ConsoleBinding::runCommand(lua_State* L) {
    const haylen::debug::Console::Result result = getConsole(L).run(lua::Stack::read<std::string_view>(L, 1));
    lua::Stack::push(L, result.ok);
    lua::Stack::push(L, result.output);
    return 2;
}

// Returns `{name, help, usage}` for every command, in order of name.
int ConsoleBinding::commands(lua_State* L) {
    const std::vector<const haylen::debug::Console::Command*> all = getConsole(L).getCommands();
    lua_createtable(L, static_cast<int>(all.size()), 0);
    for (std::size_t index = 0; index < all.size(); ++index) {
        lua_createtable(L, 0, 3);
        lua::Stack::push(L, all[index]->name);
        lua_setfield(L, -2, "name");
        lua::Stack::push(L, all[index]->help);
        lua_setfield(L, -2, "help");
        lua::Stack::push(L, haylen::debug::Console::describeUsage(*all[index]));
        lua_setfield(L, -2, "usage");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

int ConsoleBinding::consoleEnabled(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getPlugin<haylen::plugins::DebugPlugin>().isConsoleEnabled());
    return 1;
}

void ConsoleBinding::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"addCommand", &lua::Binding::native<&addCommand>}, {"removeCommand", &lua::Binding::native<&removeCommand>}, {"runCommand", &lua::Binding::native<&runCommand>}, {"commands", &lua::Binding::native<&commands>}, {"consoleEnabled", &lua::Binding::native<&consoleEnabled>}, {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

} // namespace haylen::lua::debug
