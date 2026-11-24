#pragma once

#include <lua.hpp>

#include <cstddef>
#include <memory>

#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "runtime/Probe.hpp"

namespace haylen::lua {

// Binds `Probe` with the toolkit the bindings of the engine use, together with a function that takes the shared object.
class ProbeLua final {
  public:
    static void install(lua_State* L) {
        ClassBuilder<Probe>(L).field<&Probe::speed>("speed").field<&Probe::position>("position").field<&Probe::tint>("tint").field<&Probe::visible>("visible").nestedField<&Probe::order, &Probe::Order::depth>("depth").nestedField<&Probe::order, &Probe::Order::layer>("layer").accessor<&Probe::getScale, &Probe::setScale>("scale").indexer(&at, &assign).install();
        lua_register(L, "probeSpeed", &Binding::function<&speedOf>);
    }

  private:
    static float speedOf(const std::shared_ptr<Probe>& probe) {
        return probe->speed;
    }

    static std::size_t checkIndex(lua_State* L) {
        const lua_Integer index = luaL_checkinteger(L, 2);
        luaL_argcheck(L, index >= 1 && index <= 2, 2, "index out of range");
        return static_cast<std::size_t>(index - 1);
    }

    static int at(lua_State* L) {
        lua_pushnumber(L, static_cast<lua_Number>(Userdata::check<Probe>(L, 1).values[checkIndex(L)]));
        return 1;
    }

    static int assign(lua_State* L) {
        Userdata::check<Probe>(L, 1).values[checkIndex(L)] = static_cast<float>(luaL_checknumber(L, 3));
        return 0;
    }
};

} // namespace haylen::lua
