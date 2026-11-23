#include "2d/lighting/Lighting2DBinding.hpp"

#include <cstdint>

#include "2d/lighting/LightBinding.hpp"
#include "2d/lighting/OccluderBinding.hpp"
#include "haylen/2d/lighting/LightFlicker.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"

namespace haylen::lua::lighting2d {

// Returns a flame-like intensity multiplier between `1 - amount` and 1 with `flicker(time, {speed = 8, amount = 0.15, seed = 0})`.
int Lighting2DBinding::flicker(lua_State* L) {
    const auto time = lua::Stack::read<float>(L, 1);
    float speed = 8.0F;
    float amount = 0.15F;
    lua_Integer seed = 0;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kFlickerFields});
        lua::Table::readField(L, 2, "speed", speed);
        lua::Table::readField(L, 2, "amount", amount);
        lua::Table::readField(L, 2, "seed", seed);
    }

    lua::Stack::push(L, haylen::lighting2d::LightFlicker::intensity(time, speed, amount, static_cast<std::uint64_t>(seed)));
    return 1;
}

int Lighting2DBinding::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"flicker", &lua::Binding::native<&flicker>}, {"newLight", &lua::Binding::native<&LightBinding::newLight>}, {"illuminate", &lua::Binding::native<&LightBinding::illuminate>}, {"falloff", &lua::Binding::native<&LightBinding::falloff>}, {"newOccluder", &lua::Binding::native<&OccluderBinding::newOccluder>}, {"occludersFromBody", &lua::Binding::native<&OccluderBinding::fromBody>}, {"occludersFromMap", &lua::Binding::native<&OccluderBinding::fromMap>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void Lighting2DBinding::install(lua_State* L) {
    LightBinding::install(L);
    OccluderBinding::install(L);
    lua::Binding::preload(L, "haylen.lighting2d", &open);
}

} // namespace haylen::lua::lighting2d
