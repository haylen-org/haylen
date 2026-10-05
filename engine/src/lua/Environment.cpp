#include "lua/Environment.hpp"

#include <monocypher.h>

#include <algorithm>
#include <format>
#include <limits>
#include <stdexcept>
#include <string_view>
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

// The outermost frames without a name are the native entries of Varn that ran the task or the callback, which the stack leaves out like the protected calls of the engine.
std::vector<Error::Frame> Environment::readFrames(lua_State* L, int index) {
    std::vector<Error::Frame> frames;
    const lua_Integer count = luaL_len(L, index);
    for (lua_Integer position = 1; position <= count; ++position) {
        lua_rawgeti(L, index, position);
        frames.push_back(readFrame(L, lua_gettop(L)));
        lua_pop(L, 1);
    }

    while (!frames.empty() && frames.back().kind == Error::Frame::Kind::C && frames.back().function == "?") {
        frames.pop_back();
    }
    return frames;
}

// Varn lists a level with the fields that `lua_getinfo` fills, leaving out those the level has none of, and stands for the middle of a deep stack with one frame of the kind `skipped`, so the frames read like those of the protected calls.
Error::Frame Environment::readFrame(lua_State* L, int index) {
    const std::string what = readText(L, index, "kind");
    if (what == "skipped") {
        return Runtime::makeSkippedFrame(readInteger(L, index, "count"));
    }

    const std::string source = readText(L, index, "source");
    const std::string nameWhat = readText(L, index, "namewhat");
    const std::string name = readText(L, index, "name");
    return Runtime::makeFrame({.source = source, .line = readInteger(L, index, "line"), .what = what, .nameWhat = nameWhat, .name = name, .lineDefined = readInteger(L, index, "linedefined")});
}

std::string Environment::readText(lua_State* L, int index, const char* key) {
    lua_getfield(L, index, key);
    std::string text = lua_type(L, -1) == LUA_TSTRING ? lua_tostring(L, -1) : "";
    lua_pop(L, 1);
    return text;
}

int Environment::readInteger(lua_State* L, int index, const char* key) {
    lua_getfield(L, index, key);
    const auto value = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    return value;
}

// Receives the value that was raised, the traceback as text, the frames of where it was raised and the kind of the failure. A callback called from another thread, a closing variable of a cancelled task and a promise that native code rejected carry no frames.
int Environment::reportFailure(lua_State* L) {
    std::vector<Error::Frame> frames;
    if (lua_istable(L, 3)) {
        frames = readFrames(L, 3);
    }
    Error error(Runtime::describeValue(L, 1), std::move(frames));
    if (lua_type(L, 4) == LUA_TSTRING && std::string_view(lua_tostring(L, 4)) == "rejection") {
        error = describeRejection(error);
    }
    const core::Engine::PhaseScope scope(Runtime::getEngine(L), core::Engine::Phase::Lifecycle);
    Runtime::reportError(L, error);
    return 0;
}

// The message of an unobserved rejection says that nothing awaited the promise, and the error keeps the file and line that its text or its frames gave.
Error Environment::describeRejection(const Error& error) {
    const std::string message = "A promise rejected, and nothing awaited or caught it. " + error.getMessage();
    if (error.getMessage() == error.what()) {
        return Error(message, error.getFrames());
    }
    return Error(std::format("{}:{}: {}", error.getFile(), error.getLine(), message), error.getFrames());
}

// Varn hands the failures that no caller receives to the one handler of `async.onFailure`, which shows them on the error screen: those of the tasks of `async.spawn` and `async.run`, of the to-be-closed variables of cancelled tasks, of `ffi` callbacks that fail outside any `ffi` call, of the handlers of its HTTP and WebSocket servers and the callbacks of `http.client.stream`, and the rejections of promises that nothing awaited or caught.
void Environment::installFailureHandler(lua_State* L) {
    lua_getglobal(L, "require");
    lua_pushliteral(L, "async");
    Runtime::protectedCall(L, 1, 1);
    lua_getfield(L, -1, "onFailure");
    lua_pushcfunction(L, &Binding::native<&reportFailure>);
    Runtime::protectedCall(L, 1, 0);
    lua_pop(L, 1);
}

int Environment::loadModule(lua_State* L, const io::Package& package, const std::string& path) {
    std::vector<std::uint8_t> bytes = package.read(path);
    const bool bytecode = package.isLuaBytecode(path);
    const std::string chunkName = "@" + path;
    const int status = luaL_loadbufferx(L, reinterpret_cast<const char*>(bytes.data()), bytes.size(), chunkName.c_str(), bytecode ? "b" : "t");
    if (bytecode) {
        crypto_wipe(bytes.data(), bytes.size());
    }
    return status;
}

void Environment::runModule(lua_State* L, const io::Package& package, const std::string& path) {
    if (loadModule(L, package, path) != LUA_OK) {
        Error error = Runtime::readError(L, -1);
        lua_pop(L, 1);
        throw error;
    }
    Runtime::protectedCall(L, 0, 0);
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
            if (loadModule(L, package, candidate) != LUA_OK) {
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

int Environment::callOriginal(lua_State* L) {
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_insert(L, 1);
    lua_call(L, lua_gettop(L) - 1, LUA_MULTRET);
    return lua_gettop(L);
}

int Environment::setUnprotectedMetatable(lua_State* L) {
    if (luaL_getmetafield(L, 1, "__metatable") != LUA_TNIL) {
        return luaL_error(L, "The metatable of this value is protected and cannot be changed.");
    }
    return callOriginal(L);
}

// Native functions show no upvalues, which hold the method tables and state of engine types.
int Environment::accessScriptUpvalue(lua_State* L) {
    if (lua_iscfunction(L, 1) != 0) {
        return 0;
    }
    return callOriginal(L);
}

// The functions of the debug library that take a level count it from the script that calls them, so a level of the running thread goes on shifted by one, past the wrapper that calls them.
bool Environment::shiftLevel(lua_State* L) {
    const int index = lua_type(L, 1) == LUA_TTHREAD ? 2 : 1;
    int isInteger = 0;
    const lua_Integer level = lua_tointegerx(L, index, &isInteger);
    if (isInteger == 0 || level < 0 || level >= std::numeric_limits<int>::max()) {
        return false;
    }
    lua_State* target = index == 2 ? lua_tothread(L, 1) : L;
    lua_Debug info{};
    const bool native = lua_getstack(target, static_cast<int>(level), &info) != 0 && lua_getinfo(target, "S", &info) != 0 && std::string_view(info.what) == "C";
    if (target == L) {
        lua_pushinteger(L, level + 1);
        lua_replace(L, index);
    }
    return native;
}

// The stack slots of a native function hold the state of the engine while it runs, so scripts read and change the locals of Lua functions only.
int Environment::getScriptLocal(lua_State* L) {
    if (shiftLevel(L)) {
        luaL_pushfail(L);
        return 1;
    }
    return callOriginal(L);
}

int Environment::setScriptLocal(lua_State* L) {
    if (shiftLevel(L)) {
        return luaL_error(L, "The locals of native functions cannot be changed.");
    }
    return callOriginal(L);
}

// The function of a native frame can be an internal function of the engine, which only runs with the arguments the engine gives it, so the information of a native frame leaves it out.
int Environment::getScriptInfo(lua_State* L) {
    const bool native = shiftLevel(L);
    const int results = callOriginal(L);
    if (native && lua_istable(L, -1)) {
        lua_pushnil(L);
        lua_setfield(L, -2, "func");
    }
    return results;
}

// The debug library would otherwise reach around the protected metatables of the engine, whose finalizers and native properties must never run on the wrong object, and into the state that native functions and engine values keep. Scripts make no userdata of their own, so the user values of every userdata stay out of reach.
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
    for (const auto& [name, wrapper] : {std::pair<const char*, lua_CFunction>{"getlocal", &getScriptLocal}, {"setlocal", &setScriptLocal}, {"getinfo", &getScriptInfo}}) {
        lua_getfield(L, -1, name);
        lua_pushcclosure(L, wrapper, 1);
        lua_setfield(L, -2, name);
    }
    for (const char* name : {"getregistry", "getuservalue", "setuservalue"}) {
        lua_pushnil(L);
        lua_setfield(L, -2, name);
    }
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
