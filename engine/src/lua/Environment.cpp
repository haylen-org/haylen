#include "lua/Environment.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/io/Path.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Runtime.hpp"

namespace haylen::lua {

std::string Environment::modulePath(std::string_view name) {
    std::string path(name);
    for (char& character : path) {
        if (character == '.') {
            character = '/';
        }
    }
    return path;
}

// A module whose first name part is the id of a plugin of the app comes from the source folder of that plugin, where the id alone names `init.lua`. Every other module comes from the source folder of the app.
std::vector<std::string> Environment::getCandidates(const core::AppConfig& config, std::string_view name) {
    const std::string head(name.substr(0, name.find('.')));
    if (!config.plugins.contains(head)) {
        const std::string base = std::string(io::Path::kSourceDirectory) + "/" + modulePath(name);
        return {base + ".lua", base + "/init.lua"};
    }

    const std::string source = io::Path::plugin(head, io::Path::kSourceDirectory);
    if (head.size() == name.size()) {
        return {source + "/init.lua"};
    }
    const std::string base = source + "/" + modulePath(name.substr(head.size() + 1));
    return {base + ".lua", base + "/init.lua"};
}

// A module of the app whose first name part is a plugin id could never load, because `require` finds the module of the plugin first.
void Environment::checkModules(core::Engine& engine) {
    const core::AppConfig& config = engine.getConfig();
    if (config.plugins.empty()) {
        return;
    }

    const std::string root = std::string(io::Path::kSourceDirectory) + "/";
    for (const std::string& file : engine.getPackage().list(io::Path::kSourceDirectory)) {
        if (io::Path::extension(file) != ".lua") {
            continue;
        }
        std::string module = file.substr(root.size());
        module.resize(module.size() - std::string_view(".lua").size());
        if (module.ends_with("/init")) {
            module.resize(module.size() - std::string_view("/init").size());
        }
        std::ranges::replace(module, '/', '.');

        const std::string id = module.substr(0, module.find('.'));
        if (config.plugins.contains(id)) {
            throw std::runtime_error("The app module \"" + file + "\" has the name \"" + module + "\", which \"require\" resolves to \"" + getCandidates(config, module).front() + "\" of the plugin \"" + id + "\". Rename the module of the app.");
        }
    }
}

// Varn gives the name of a function without the way the code reached it, and the outermost frames without a name are the native entries of Varn that ran the task or the callback, which the stack leaves out like the protected calls of the engine.
std::vector<Error::Frame> Environment::readFrames(lua_State* L, int index) {
    std::vector<Error::Frame> frames;
    const lua_Integer count = luaL_len(L, index);
    for (lua_Integer position = 1; position <= count; ++position) {
        lua_rawgeti(L, index, position);
        lua_getfield(L, -1, "source");
        lua_getfield(L, -2, "line");
        lua_getfield(L, -3, "name");
        lua_getfield(L, -4, "kind");
        const Error::Frame::Kind kind = Runtime::getFrameKind(lua_tostring(L, -1));
        std::string function = kind == Error::Frame::Kind::Main ? "main chunk" : "?";
        if (lua_isstring(L, -2) != 0) {
            function = "function '" + std::string(lua_tostring(L, -2)) + "'";
        }
        frames.push_back({.source = lua_tostring(L, -4), .line = static_cast<int>(lua_tointeger(L, -3)), .function = std::move(function), .kind = kind});
        lua_pop(L, 5);
    }

    while (!frames.empty() && frames.back().kind == Error::Frame::Kind::C && frames.back().function == "?") {
        frames.pop_back();
    }
    return frames;
}

// Receives the value that was raised, the traceback as text and the frames of where it was raised, which a callback called from another thread has none of.
int Environment::reportFailure(lua_State* L) {
    std::vector<Error::Frame> frames;
    if (lua_istable(L, 3)) {
        frames = readFrames(L, 3);
    }
    Runtime::reportError(L, Error(Runtime::describeValue(L, 1), std::move(frames)));
    return 0;
}

// Varn hands the failures that no caller receives, those of the tasks of `async.spawn` and `async.run` and of `ffi` callbacks that fail outside any `ffi` call, to the one handler of `async.onFailure`, which shows them on the error screen.
void Environment::installFailureHandler(lua_State* L) {
    lua_getglobal(L, "require");
    lua_pushliteral(L, "async");
    Runtime::protectedCall(L, 1, 1);
    lua_getfield(L, -1, "onFailure");
    lua_pushcfunction(L, &Binding::native<&reportFailure>);
    Runtime::protectedCall(L, 1, 0);
    lua_pop(L, 1);
}

// Searches the package for `name.lua` and then `name/init.lua`, the same order Lua uses on disk.
int Environment::searchPackage(lua_State* L) {
    // clang-format off
    return Binding::guarded(L, [L] {
        const std::string name = luaL_checkstring(L, 1);
        core::Engine& engine = Runtime::getEngine(L);
        io::Package& package = engine.getPackage();
        const std::vector<std::string> candidates = getCandidates(engine.getConfig(), name);

        for (const std::string& candidate : candidates) {
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

        std::string message = "no file '" + candidates.front() + "'";
        if (candidates.size() > 1) {
            message += " or '" + candidates.back() + "'";
        }
        message += " in the app package";
        lua_pushstring(L, message.c_str());
        return 1;
    });
    // clang-format on
}

int Environment::loadAsText(lua_State* L) {
    const auto mode = static_cast<int>(lua_tointeger(L, lua_upvalueindex(2)));
    if (!lua_isnoneornil(L, mode) && std::string_view(luaL_checkstring(L, mode)) != "t") {
        return luaL_argerror(L, mode, "chunks load only as text, so the mode is 't'");
    }

    // An environment argument after the mode counts even when it is `nil`, so the arguments keep the count the caller gave.
    lua_settop(L, std::max(lua_gettop(L), mode));
    lua_pushliteral(L, "t");
    lua_replace(L, mode);
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_insert(L, 1);
    lua_call(L, lua_gettop(L) - 1, LUA_MULTRET);
    return lua_gettop(L);
}

int Environment::doFileAsText(lua_State* L) {
    const char* name = luaL_optstring(L, 1, nullptr);
    lua_settop(L, 1);
    if (luaL_loadfilex(L, name, "t") != LUA_OK) {
        return lua_error(L);
    }
    lua_callk(L, 0, LUA_MULTRET, 0, &finishDoFile);
    return finishDoFile(L, LUA_OK, 0);
}

int Environment::finishDoFile(lua_State* L, int, lua_KContext) {
    return lua_gettop(L) - 1;
}

void Environment::wrapLoader(lua_State* L, const char* name, int modeIndex) {
    lua_getglobal(L, name);
    lua_pushinteger(L, modeIndex);
    lua_pushcclosure(L, &loadAsText, 2);
    lua_setglobal(L, name);
}

// Bytecode can craft values the virtual machine never checks, so app code loads chunks only as text, like the engine.
void Environment::restrictLoading(lua_State* L) {
    wrapLoader(L, "load", kLoadMode);
    wrapLoader(L, "loadfile", kLoadFileMode);
    lua_pushcfunction(L, &doFileAsText);
    lua_setglobal(L, "dofile");

    lua_getglobal(L, "string");
    lua_pushnil(L);
    lua_setfield(L, -2, "dump");
    lua_pop(L, 1);
}

int Environment::setUnprotectedMetatable(lua_State* L) {
    if (luaL_getmetafield(L, 1, "__metatable") != LUA_TNIL) {
        return luaL_error(L, "The metatable of this value is protected and cannot be changed.");
    }
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_insert(L, 1);
    lua_call(L, lua_gettop(L) - 1, LUA_MULTRET);
    return lua_gettop(L);
}

// Native functions show no upvalues, which hold the method tables and state of engine types.
int Environment::accessScriptUpvalue(lua_State* L) {
    if (lua_iscfunction(L, 1) != 0) {
        return 0;
    }
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_insert(L, 1);
    lua_call(L, lua_gettop(L) - 1, LUA_MULTRET);
    return lua_gettop(L);
}

// The debug library would otherwise reach around the protected metatables of the engine, whose finalizers and native properties must never run on the wrong object.
void Environment::restrictDebug(lua_State* L) {
    lua_getglobal(L, "debug");
    lua_getglobal(L, "getmetatable");
    lua_setfield(L, -2, "getmetatable");
    lua_getfield(L, -1, "setmetatable");
    lua_pushcclosure(L, &setUnprotectedMetatable, 1);
    lua_setfield(L, -2, "setmetatable");
    for (const char* name : {"getupvalue", "setupvalue"}) {
        lua_getfield(L, -1, name);
        lua_pushcclosure(L, &accessScriptUpvalue, 1);
        lua_setfield(L, -2, name);
    }
    lua_pushnil(L);
    lua_setfield(L, -2, "getregistry");
    lua_pop(L, 1);
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

    installFailureHandler(L);
    restrictLoading(L);
    restrictDebug(L);
}

} // namespace haylen::lua
