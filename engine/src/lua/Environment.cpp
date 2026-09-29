#include "lua/Environment.hpp"

#include <lua.hpp>

#include <stdexcept>

#include "haylen/core/Engine.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/io/Path.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Runtime.hpp"

namespace haylen::lua {

// Varn only logs an error that escapes a task of async.spawn or async.run, so the engine runs every task in a protected call with its own message handler and shows the error on the error screen with the stack of the task.
const std::string_view Environment::kTaskErrors = R"lua(
local async, report, handleMessage = ...
for _, name in ipairs({'spawn', 'run'}) do
    local start = async[name]
    async[name] = function(task)
        if type(task) ~= 'function' then
            error("bad argument #1 to '" .. name .. "' (function expected, got " .. type(task) .. ')', 2)
        end
        start(function()
            local ok, failure = xpcall(task, handleMessage)
            if not ok then
                report(failure)
            end
        end)
    end
end
)lua";

std::string Environment::modulePath(std::string_view name) {
    std::string path(name);
    for (char& character : path) {
        if (character == '.') {
            character = '/';
        }
    }
    return path;
}

int Environment::reportTaskError(lua_State* L) {
    Runtime::reportError(L, Runtime::readError(L, 1));
    return 0;
}

void Environment::installTaskErrors(lua_State* L) {
    if (luaL_loadbufferx(L, kTaskErrors.data(), kTaskErrors.size(), Runtime::kTaskChunk, "t") != LUA_OK) {
        throw std::runtime_error(lua_tostring(L, -1));
    }
    lua_getglobal(L, "require");
    lua_pushliteral(L, "async");
    Runtime::protectedCall(L, 1, 1);
    lua_pushcfunction(L, &reportTaskError);
    lua_pushcfunction(L, &Runtime::handleMessage);
    Runtime::protectedCall(L, 3, 0);
}

// Searches the source folder of the package for name.lua and then name/init.lua, the same order Lua uses on disk.
int Environment::searchPackage(lua_State* L) {
    // clang-format off
    return Binding::guarded(L, [L] {
        const std::string name = luaL_checkstring(L, 1);
        io::Package& package = Runtime::getEngine(L).getPackage();
        const std::string base = std::string(io::Path::kSourceDirectory) + "/" + modulePath(name);

        for (const std::string& candidate : {base + ".lua", base + "/init.lua"}) {
            if (!package.exists(candidate)) {
                continue;
            }

            const std::string source = package.readText(candidate);
            const std::string chunkName = "@" + candidate;
            if (luaL_loadbufferx(L, source.data(), source.size(), chunkName.c_str(), "t") != LUA_OK) {
                return lua_error(L);
            }
            lua_pushstring(L, candidate.c_str());
            return 2;
        }

        lua_pushfstring(L, "no file '%s.lua' or '%s/init.lua' in the app package", base.c_str(), base.c_str());
        return 1;
    });
    // clang-format on
}

void Environment::install(core::Engine& engine, lua_State* L) {
    lua_pushlightuserdata(L, &engine);
    lua_setfield(L, LUA_REGISTRYINDEX, Runtime::kEngineKey);

    // Only preloaded native modules and package modules can be required, so an app behaves the same from a folder, a zip or the browser.
    lua_getglobal(L, "package");
    lua_createtable(L, 2, 0);
    lua_getfield(L, -2, "searchers");
    lua_rawgeti(L, -1, 1);
    lua_rawseti(L, -3, 1);
    lua_pop(L, 1);
    lua_pushcfunction(L, &searchPackage);
    lua_rawseti(L, -2, 2);
    lua_setfield(L, -2, "searchers");

    lua_pushliteral(L, "");
    lua_setfield(L, -2, "path");
    lua_pushliteral(L, "");
    lua_setfield(L, -2, "cpath");
    lua_pop(L, 1);

    installTaskErrors(L);
}

} // namespace haylen::lua
