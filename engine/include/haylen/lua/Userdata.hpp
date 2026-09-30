#pragma once

#include <lua.hpp>

#include <memory>
#include <new>
#include <utility>

#include "haylen/debug/ObjectCounter.hpp"
#include "haylen/lua/Type.hpp"

namespace haylen::lua {

// Creates and checks the userdata that hold bound C++ objects in Lua.
class Userdata final {
  public:
    template <Bound T> [[nodiscard]] static T* test(lua_State* L, int index) {
        auto* storage = testStorage<T>(L, index);
        if (storage == nullptr) {
            return nullptr;
        }
        if constexpr (SharedBound<T>) {
            return storage->get();
        } else if constexpr (WeakBound<T>) {
            return storage->lock().get();
        } else {
            return storage;
        }
    }

    // The owner of a weakly bound object keeps it alive for as long as the frame thread uses it, so the reference outlives the lock.
    template <Bound T> [[nodiscard]] static T& check(lua_State* L, int index) {
        auto* storage = static_cast<typename Type<T>::Storage*>(luaL_checkudata(L, index, Type<T>::name));
        if constexpr (SharedBound<T>) {
            if (!*storage) {
                luaL_error(L, "This \"%s\" was already released.", Type<T>::name);
            }
            return **storage;
        } else if constexpr (WeakBound<T>) {
            T* object = storage->lock().get();
            if (object == nullptr) {
                luaL_error(L, "This \"%s\" was already released.", Type<T>::name);
            }
            return *object;
        } else {
            return *storage;
        }
    }

    template <SharedBound T> [[nodiscard]] static const std::shared_ptr<T>& checkShared(lua_State* L, int index) {
        auto* storage = static_cast<std::shared_ptr<T>*>(luaL_checkudata(L, index, Type<T>::name));
        if (!*storage) {
            luaL_error(L, "This \"%s\" was already released.", Type<T>::name);
        }
        return *storage;
    }

    template <Bound T, typename... Args> static typename Type<T>::Storage& emplace(lua_State* L, Args&&... args) {
        using Storage = typename Type<T>::Storage;
        void* memory = lua_newuserdatauv(L, sizeof(Storage), 1);
        auto* storage = new (memory) Storage(std::forward<Args>(args)...);
        luaL_setmetatable(L, Type<T>::name);
        getCounter<T>().add();
        return *storage;
    }

    // The `__eq` metamethod of a type whose values compare with `==`, so two userdata of the same resource or of the same shared object are equal, and a value of another type never is.
    template <Bound T> static int equal(lua_State* L) {
        const auto* first = testStorage<T>(L, 1);
        const auto* second = testStorage<T>(L, 2);
        lua_pushboolean(L, first != nullptr && second != nullptr && *first == *second ? 1 : 0);
        return 1;
    }

    // Counts the userdata of the type that Lua created and collected, which the debug statistics list by the type name.
    template <Bound T> [[nodiscard]] static debug::ObjectCounter& getCounter() {
        static debug::ObjectCounter& counter = *new debug::ObjectCounter(Type<T>::name, debug::ObjectCounter::Kind::Userdata);
        return counter;
    }

    // Pushes the metatable registered under `name` and returns whether it was created, like `luaL_newmetatable`. A new metatable is protected by its `__metatable` field, so `getmetatable` returns the name and Lua code can neither reach nor replace the metatable, its finalizer or its native properties.
    static bool newMetatable(lua_State* L, const char* name);

    // Userdata keep callbacks and other Lua values in a table stored as their first user value, so a value that refers back to its owner never keeps it alive forever.
    static void pushField(lua_State* L, int index, const char* name);
    static void setField(lua_State* L, int index, const char* name, int valueIndex);

    // Pushes the function stored under `name` and returns `true`, or leaves the stack unchanged and returns `false`.
    static bool pushFunction(lua_State* L, int index, const char* name);

  private:
    template <Bound T> [[nodiscard]] static typename Type<T>::Storage* testStorage(lua_State* L, int index) {
        return static_cast<typename Type<T>::Storage*>(luaL_testudata(L, index, Type<T>::name));
    }
};

} // namespace haylen::lua
