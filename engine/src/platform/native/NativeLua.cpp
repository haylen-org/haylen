#include "platform/native/NativeLua.hpp"

#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Reference.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/platform/NativeLibraries.hpp"
#include "platform/native/NativeApi.hpp"
#include "platform/native/NativeCallbacks.hpp"
#include "platform/native/NativeSignature.hpp"
#include "plugins/NativePlugin.hpp"

namespace haylen::platform {

int NativeLua::available(lua_State* L) {
    lua_pushboolean(L, NativeLibraries::isAvailable() ? 1 : 0);
    return 1;
}

// Loads a library with `load(name or path, {init = 'symbol', global = false})` and returns the namespace of Varn's `ffi` that calls its declared functions. A library linked into the app returns `ffi.C`, where its symbols live.
int NativeLua::load(lua_State* L) {
    const std::string name = lua::Stack::read<std::string>(L, 1);
    std::string init;
    bool global = false;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kLoadOptions});
        lua::Table::readField(L, 2, "init", init);
        lua::Table::readField(L, 2, "global", global);
    }

    const NativeLibraries::Library library = NativeLibraries::open(name);
    if (!init.empty()) {
        void* address = NativeLibraries::findSymbol(library, init);
        if (address == nullptr) {
            throw std::runtime_error("The native library \"" + name + "\" has no function \"" + init + "\".");
        }
        const int code = reinterpret_cast<HaylenNativeInit>(address)(&NativeApi::get());
        if (code != 0) {
            throw std::runtime_error("The function \"" + init + "\" of the native library \"" + name + "\" failed with code " + std::to_string(code) + ".");
        }
    }

    pushFfi(L);
    if (library.linked) {
        lua_getfield(L, -1, "C");
        return 1;
    }
    lua_getfield(L, -1, "load");
    lua::Stack::push(L, library.path);
    lua_pushboolean(L, global ? 1 : 0);
    lua_call(L, 2, 1);
    return 1;
}

// Returns the address of a symbol as a light userdata that `ffi.cast` turns into a typed pointer, or `nil`.
int NativeLua::findSymbol(lua_State* L) {
    void* address = NativeLibraries::findSymbol(lua::Stack::read<std::string_view>(L, 1));
    if (address == nullptr) {
        lua_pushnil(L);
    } else {
        lua_pushlightuserdata(L, address);
    }
    return 1;
}

// Creates a callback with `callback(declaration, function, {thread = 'any' or 'frame'})`. Its pointer goes to native code, and the function receives the copied arguments on the frame thread.
int NativeLua::callback(lua_State* L) {
    NativeSignature signature = NativeSignature::parse(lua::Stack::read<std::string_view>(L, 1));
    luaL_checktype(L, 2, LUA_TFUNCTION);
    std::string thread = "any";
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua::Table::checkFields(L, 3, {kCallbackOptions});
        lua::Table::readField(L, 3, "thread", thread);
    }
    if (thread != "any" && thread != "frame") {
        throw std::invalid_argument("The thread of a native callback is \"any\" or \"frame\", not \"" + thread + "\".");
    }

    core::Engine& engine = lua::Runtime::getEngine(L);
    plugins::NativePlugin& plugin = engine.getPlugin<plugins::NativePlugin>();
    const std::uint64_t id = plugin.getCallbacks()->add(lua::Reference(L, 2));
    NativeCallback::Link link{.owner = plugin.getCallbacks(), .mailbox = engine.getPlatform().getMailbox(), .frameThread = std::this_thread::get_id(), .id = id};
    try {
        lua::Userdata::emplace<NativeCallback>(L, NativeCallback::create(std::move(signature), thread == "frame" ? NativeCallback::Thread::Frame : NativeCallback::Thread::Any, std::move(link)));
    } catch (...) {
        plugin.getCallbacks()->remove(id);
        throw;
    }
    return 1;
}

std::shared_ptr<NativeCallback>& NativeLua::getStorage(lua_State* L) {
    return *static_cast<std::shared_ptr<NativeCallback>*>(luaL_checkudata(L, 1, lua::Type<NativeCallback>::name));
}

int NativeLua::getPointer(lua_State* L) {
    lua_pushlightuserdata(L, lua::Userdata::check<NativeCallback>(L, 1).getAddress());
    return 1;
}

int NativeLua::isFreed(lua_State* L) {
    lua_pushboolean(L, getStorage(L) == nullptr ? 1 : 0);
    return 1;
}

// Frees the function pointer, which native code must no longer call. Freeing twice does nothing.
int NativeLua::freeCallback(lua_State* L) {
    std::shared_ptr<NativeCallback>& storage = getStorage(L);
    if (storage == nullptr) {
        return 0;
    }
    lua::Runtime::getEngine(L).getPlugin<plugins::NativePlugin>().getCallbacks()->remove(storage->getId());
    NativeCallback::release(storage);
    storage.reset();
    return 0;
}

void NativeLua::pushFfi(lua_State* L) {
    luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
    lua_getfield(L, -1, "ffi");
    lua_remove(L, -2);
}

int NativeLua::open(lua_State* L) {
    lua::ClassBuilder<NativeCallback>(L).property("pointer", &getPointer).property("freed", &isFreed).function("free", &lua::Binding::native<&freeCallback>).install();

    const luaL_Reg functions[] = {
        {"available", &available}, {"load", &lua::Binding::native<&load>}, {"findSymbol", &lua::Binding::native<&findSymbol>}, {"callback", &lua::Binding::native<&callback>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void NativeLua::install(lua_State* L) {
    lua::Binding::preload(L, "haylen.native", &open);
}

} // namespace haylen::platform
