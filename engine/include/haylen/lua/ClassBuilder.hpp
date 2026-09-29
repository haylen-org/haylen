#pragma once

#include <lua.hpp>

#include <tuple>
#include <type_traits>

#include "haylen/lua/Binding.hpp"
#include "haylen/lua/FunctionTraits.hpp"
#include "haylen/lua/NativeProperty.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua {

// Builds the metatable of a bound type with methods, read-write properties and metamethods. Unknown keys raise errors. Number, Vec2 and Color fields and accessors are also recorded as native properties, which tweens animate without running Lua.
template <Bound T> class ClassBuilder final {
  public:
    explicit ClassBuilder(lua_State* L) : state(L) {
        luaL_newmetatable(L, Type<T>::name);
        metatable = lua_gettop(L);
        lua_newtable(L);
        methods = lua_gettop(L);
        lua_newtable(L);
        getters = lua_gettop(L);
        lua_newtable(L);
        setters = lua_gettop(L);
    }

    ClassBuilder(const ClassBuilder&) = delete;
    ClassBuilder& operator=(const ClassBuilder&) = delete;

    ClassBuilder& function(const char* name, lua_CFunction callable) {
        lua_pushcfunction(state, callable);
        lua_setfield(state, methods, name);
        return *this;
    }

    template <auto Method> ClassBuilder& method(const char* name) {
        return function(name, &Binding::method<Method>);
    }

    ClassBuilder& property(const char* name, lua_CFunction getter, lua_CFunction setter = nullptr) {
        lua_pushcfunction(state, getter);
        lua_setfield(state, getters, name);
        if (setter != nullptr) {
            lua_pushcfunction(state, setter);
            lua_setfield(state, setters, name);
        }
        return *this;
    }

    template <auto Member> ClassBuilder& field(const char* name) {
        using Field = typename FunctionTraits<decltype(Member)>::Field;
        if constexpr (NativeProperty::kindOf<Field>().has_value()) {
            NativeProperty::record(state, metatable, name, fieldProperty<Member>);
        }
        return property(name, &Binding::getField<Member>, &Binding::setField<Member>);
    }

    // Exposes a getter and setter pair of member functions as a read-write property.
    template <auto Getter, auto Setter> ClassBuilder& accessor(const char* name) {
        using Value = std::remove_cvref_t<typename FunctionTraits<decltype(Getter)>::Result>;
        if constexpr (NativeProperty::kindOf<Value>().has_value()) {
            NativeProperty::record(state, metatable, name, accessorProperty<Getter, Setter>);
        }
        return property(name, &Binding::method<Getter>, &setThrough<Setter>);
    }

    template <auto Member> ClassBuilder& readOnlyField(const char* name) {
        return property(name, &Binding::getField<Member>);
    }

    template <auto Outer, auto Inner> ClassBuilder& nestedField(const char* name) {
        using Field = typename FunctionTraits<decltype(Inner)>::Field;
        if constexpr (NativeProperty::kindOf<Field>().has_value()) {
            NativeProperty::record(state, metatable, name, nestedProperty<Outer, Inner>);
        }
        return property(name, &Binding::getNestedField<Outer, Inner>, &Binding::setNestedField<Outer, Inner>);
    }

    // Sends integer keys to the getter and the setter before any method or property, such as the values of a buffer. They receive the object, the key and, for the setter, the value.
    ClassBuilder& indexer(lua_CFunction getter, lua_CFunction setter) {
        indexGetter = getter;
        indexSetter = setter;
        return *this;
    }

    ClassBuilder& meta(const char* name, lua_CFunction callable) {
        lua_pushcfunction(state, callable);
        lua_setfield(state, metatable, name);
        return *this;
    }

    void install() {
        lua_pushcfunction(state, &collect);
        lua_setfield(state, metatable, "__gc");

        lua_pushvalue(state, methods);
        lua_pushvalue(state, getters);
        if (indexGetter != nullptr) {
            lua_pushcfunction(state, indexGetter);
            lua_pushcclosure(state, &indexWithIntegers, 3);
        } else {
            lua_pushcclosure(state, &index, 2);
        }
        lua_setfield(state, metatable, "__index");

        lua_pushvalue(state, setters);
        if (indexSetter != nullptr) {
            lua_pushcfunction(state, indexSetter);
            lua_pushcclosure(state, &newIndexWithIntegers, 2);
        } else {
            lua_pushcclosure(state, &newIndex, 1);
        }
        lua_setfield(state, metatable, "__newindex");

        lua_settop(state, metatable - 1);
    }

  private:
    // Returns the object a userdata block of the type holds, or null when a shared object was released or the owner of a weak one destroyed it.
    [[nodiscard]] static T* objectOf(void* storage) noexcept {
        auto* stored = static_cast<typename Type<T>::Storage*>(storage);
        if constexpr (SharedBound<T>) {
            return stored->get();
        } else if constexpr (WeakBound<T>) {
            return stored->lock().get();
        } else {
            return stored;
        }
    }

    template <auto Member> static bool readField(void* storage, NativeProperty::Values& values) {
        T* object = objectOf(storage);
        if (object == nullptr) {
            return false;
        }
        NativeProperty::pack(object->*Member, values);
        return true;
    }

    template <auto Member> static bool writeField(void* storage, const NativeProperty::Values& values) {
        using Field = typename FunctionTraits<decltype(Member)>::Field;
        T* object = objectOf(storage);
        if (object == nullptr) {
            return false;
        }
        object->*Member = NativeProperty::unpack<Field>(values);
        return true;
    }

    template <auto Outer, auto Inner> static bool readNested(void* storage, NativeProperty::Values& values) {
        T* object = objectOf(storage);
        if (object == nullptr) {
            return false;
        }
        NativeProperty::pack((object->*Outer).*Inner, values);
        return true;
    }

    template <auto Outer, auto Inner> static bool writeNested(void* storage, const NativeProperty::Values& values) {
        using Field = typename FunctionTraits<decltype(Inner)>::Field;
        T* object = objectOf(storage);
        if (object == nullptr) {
            return false;
        }
        (object->*Outer).*Inner = NativeProperty::unpack<Field>(values);
        return true;
    }

    template <auto Getter> static bool readAccessor(void* storage, NativeProperty::Values& values) {
        T* object = objectOf(storage);
        if (object == nullptr) {
            return false;
        }
        NativeProperty::pack((object->*Getter)(), values);
        return true;
    }

    template <auto Getter, auto Setter> static bool writeAccessor(void* storage, const NativeProperty::Values& values) {
        using Value = std::remove_cvref_t<typename FunctionTraits<decltype(Getter)>::Result>;
        T* object = objectOf(storage);
        if (object == nullptr) {
            return false;
        }
        (object->*Setter)(NativeProperty::unpack<Value>(values));
        return true;
    }

    // Setters receive the object, the key and the value, the way __newindex calls them, so the value sits at index 3.
    template <auto Setter> static int setThrough(lua_State* L) {
        using Value = std::remove_cvref_t<std::tuple_element_t<0, typename FunctionTraits<decltype(Setter)>::Arguments>>;
        // clang-format off
        return Binding::guarded(L, [L] {
            (Userdata::check<T>(L, 1).*Setter)(Stack::read<Value>(L, 3));
            return 0;
        });
        // clang-format on
    }

    template <auto Member> static inline NativeProperty fieldProperty{NativeProperty::kindOf<typename FunctionTraits<decltype(Member)>::Field>().value_or(NativeProperty::Kind::Number), &readField<Member>, &writeField<Member>};
    template <auto Outer, auto Inner> static inline NativeProperty nestedProperty{NativeProperty::kindOf<typename FunctionTraits<decltype(Inner)>::Field>().value_or(NativeProperty::Kind::Number), &readNested<Outer, Inner>, &writeNested<Outer, Inner>};
    template <auto Getter, auto Setter> static inline NativeProperty accessorProperty{NativeProperty::kindOf<std::remove_cvref_t<typename FunctionTraits<decltype(Getter)>::Result>>().value_or(NativeProperty::Kind::Number), &readAccessor<Getter>, &writeAccessor<Getter, Setter>};

    // Lua code can reach __gc through the metatable, call it by hand or give the metatable to a table, so only a userdata of this type is destroyed, and only once.
    static int collect(lua_State* L) {
        using Storage = typename Type<T>::Storage;
        auto* storage = static_cast<Storage*>(luaL_testudata(L, 1, Type<T>::name));
        if (storage == nullptr) {
            return 0;
        }
        storage->~Storage();
        lua_pushnil(L);
        lua_setmetatable(L, 1);
        Userdata::getCounter<T>().remove();
        return 0;
    }

    static int index(lua_State* L) {
        lua_pushvalue(L, 2);
        if (lua_rawget(L, lua_upvalueindex(1)) != LUA_TNIL) {
            return 1;
        }
        lua_pop(L, 1);

        lua_pushvalue(L, 2);
        if (lua_rawget(L, lua_upvalueindex(2)) == LUA_TFUNCTION) {
            const lua_CFunction getter = lua_tocfunction(L, -1);
            lua_pop(L, 1);
            return getter(L);
        }
        return luaL_error(L, "%s has no member '%s'.", Type<T>::name, luaL_tolstring(L, 2, nullptr));
    }

    static int indexWithIntegers(lua_State* L) {
        if (lua_type(L, 2) == LUA_TNUMBER) {
            return lua_tocfunction(L, lua_upvalueindex(3))(L);
        }
        return index(L);
    }

    static int newIndexWithIntegers(lua_State* L) {
        if (lua_type(L, 2) == LUA_TNUMBER) {
            return lua_tocfunction(L, lua_upvalueindex(2))(L);
        }
        return newIndex(L);
    }

    static int newIndex(lua_State* L) {
        lua_pushvalue(L, 2);
        if (lua_rawget(L, lua_upvalueindex(1)) == LUA_TFUNCTION) {
            const lua_CFunction setter = lua_tocfunction(L, -1);
            lua_pop(L, 1);
            return setter(L);
        }
        return luaL_error(L, "%s has no writable property '%s'.", Type<T>::name, luaL_tolstring(L, 2, nullptr));
    }

    lua_State* state;
    int metatable = 0;
    int methods = 0;
    int getters = 0;
    int setters = 0;
    lua_CFunction indexGetter = nullptr;
    lua_CFunction indexSetter = nullptr;
};

} // namespace haylen::lua
