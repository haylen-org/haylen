#pragma once

#include <lua.hpp>

#include <optional>

#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/math/Vec3.hpp"
#include "haylen/math/Vec4.hpp"

namespace haylen::lua::math {

// Installs the `Vec3` and `Vec4` classes of `haylen.math` with their constructors and the directions of the 3D axes.
class VectorBinding final {
  public:
    static void install(lua_State* L);

    // Sets `vec3`, `vec4`, `right`, `up` and `forward` on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    static int newVec3(lua_State* L);
    static int newVec4(lua_State* L);
    static int vec3ToString(lua_State* L);
    static int vec4ToString(lua_State* L);

    template <typename Vector> static int add(lua_State* L) {
        lua::Stack::push(L, lua::Stack::read<Vector>(L, 1) + lua::Stack::read<Vector>(L, 2));
        return 1;
    }

    template <typename Vector> static int subtract(lua_State* L) {
        lua::Stack::push(L, lua::Stack::read<Vector>(L, 1) - lua::Stack::read<Vector>(L, 2));
        return 1;
    }

    // Multiplies a vector by a number on either side, or two vectors component by component.
    template <typename Vector> static int multiply(lua_State* L) {
        if (lua_type(L, 1) == LUA_TNUMBER) {
            lua::Stack::push(L, lua::Stack::read<Vector>(L, 2) * lua::Stack::read<float>(L, 1));
            return 1;
        }

        if (lua_type(L, 2) == LUA_TNUMBER) {
            lua::Stack::push(L, lua::Stack::read<Vector>(L, 1) * lua::Stack::read<float>(L, 2));
            return 1;
        }

        lua::Stack::push(L, lua::Stack::read<Vector>(L, 1) * lua::Stack::read<Vector>(L, 2));
        return 1;
    }

    template <typename Vector> static int divide(lua_State* L) {
        if (lua_type(L, 2) == LUA_TNUMBER) {
            lua::Stack::push(L, lua::Stack::read<Vector>(L, 1) / lua::Stack::read<float>(L, 2));
            return 1;
        }

        lua::Stack::push(L, lua::Stack::read<Vector>(L, 1) / lua::Stack::read<Vector>(L, 2));
        return 1;
    }

    template <typename Vector> static int negate(lua_State* L) {
        lua::Stack::push(L, -lua::Stack::read<Vector>(L, 1));
        return 1;
    }

    template <typename Vector> static int equal(lua_State* L) {
        lua_pushboolean(L, lua::Stack::read<Vector>(L, 1) == lua::Stack::read<Vector>(L, 2) ? 1 : 0);
        return 1;
    }

    template <typename Vector> [[nodiscard]] static Vector normalized(const Vector& self) {
        return self.getNormalized();
    }

    template <typename Vector> [[nodiscard]] static bool approximately(Vector self, Vector other, std::optional<float> epsilon) {
        return Vector::approximately(self, other, epsilon.value_or(1e-5F));
    }

    static int vec3Unpack(lua_State* L);
    static int vec4Unpack(lua_State* L);
};

} // namespace haylen::lua::math
