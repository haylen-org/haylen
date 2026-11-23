#include "lua/modules/DebugModule.hpp"

#include <lua.hpp>

#include <format>
#include <stdexcept>
#include <vector>

#include "core/SignalLua.hpp"
#include "debug/DebugLua.hpp"
#include "debug/ScriptInspector.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/debug/Console.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/plugins/DebugPlugin.hpp"

namespace haylen::lua::modules {

DebugModule::DebugModule() = default;

DebugModule::~DebugModule() = default;

void DebugModule::install(core::Engine& engine, lua_State* L) {
    debug::DebugLua::install(L);

    plugins::DebugPlugin& plugin = engine.getPlugin<plugins::DebugPlugin>();
    inspector = std::make_unique<debug::ScriptInspector>(L);
    plugin.setInspected(inspector.get());
    // clang-format off
    plugin.setScriptMemory([L] {
        return static_cast<std::size_t>(lua_gc(L, LUA_GCCOUNT, 0)) * 1024 + static_cast<std::size_t>(lua_gc(L, LUA_GCCOUNTB, 0));
    });
    plugin.setScriptSignals([L](std::vector<core::EventBus::Topic>& signals) {
        core::SignalLua::readNamedSignals(L, signals);
    });
    // clang-format on

    if (engine.getDevelopmentSession() == nullptr) {
        return;
    }

    // clang-format off
    command = plugin.getConsole().addCommand({.name = "lua", .help = "Runs Lua code in the app and prints what it returns.", .parameters = {{.kind = debug::Console::Kind::Text}}, .handler = [L](const core::Json& arguments) {
        return run(L, arguments[0].get_ref<const std::string&>());
    }});
    // clang-format on
}

void DebugModule::stop(core::Engine& engine) {
    command.disconnect();
    plugins::DebugPlugin& plugin = engine.getPlugin<plugins::DebugPlugin>();
    plugin.setInspected(nullptr);
    plugin.setScriptMemory({});
    plugin.setScriptSignals({});
}

// The code runs as an expression first, so `player.speed` prints its value, and as statements when it is not one.
std::string DebugModule::run(lua_State* L, const std::string& chunk) {
    const int top = lua_gettop(L);
    const std::string expression = "return " + chunk;
    if (luaL_loadbufferx(L, expression.data(), expression.size(), "=console", "t") != LUA_OK) {
        lua_pop(L, 1);
        if (luaL_loadbufferx(L, chunk.data(), chunk.size(), "=console", "t") != LUA_OK) {
            std::string message = lua_tostring(L, -1);
            lua_settop(L, top);
            throw std::invalid_argument(message);
        }
    }

    Runtime::protectedCall(L, 0, LUA_MULTRET);

    std::string text;
    for (int index = top + 1; index <= lua_gettop(L); ++index) {
        if (index > top + 1) {
            text += "  ";
        }

        switch (lua_type(L, index)) {
        case LUA_TNIL:
            text += "nil";
            break;
        case LUA_TBOOLEAN:
            text += lua_toboolean(L, index) != 0 ? "true" : "false";
            break;
        case LUA_TNUMBER:
        case LUA_TSTRING:
            text += lua_tostring(L, index);
            break;
        default:
            text += std::format("{}: {}", luaL_typename(L, index), lua_topointer(L, index));
            break;
        }
    }

    lua_settop(L, top);
    return text;
}

} // namespace haylen::lua::modules
