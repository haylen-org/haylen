#include "math/Noise3DBinding.hpp"

#include <cstdint>
#include <optional>

#include "haylen/core/EnumNames.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"

namespace haylen::core {

template <> struct EnumNames<haylen::math::Noise3D::Basis> {
    static std::optional<haylen::math::Noise3D::Basis> fromName(std::string_view name) {
        return haylen::math::Noise3D::basisFromName(name);
    }
    static std::string_view name(haylen::math::Noise3D::Basis value) {
        return haylen::math::Noise3D::basisName(value);
    }
};

template <> struct EnumNames<haylen::math::Noise3D::Fractal> {
    static std::optional<haylen::math::Noise3D::Fractal> fromName(std::string_view name) {
        return haylen::math::Noise3D::fractalFromName(name);
    }
    static std::string_view name(haylen::math::Noise3D::Fractal value) {
        return haylen::math::Noise3D::fractalName(value);
    }
};

} // namespace haylen::core

namespace haylen::lua::math {

int Noise3DBinding::newNoise(lua_State* L) {
    lua::Userdata::emplace<haylen::math::Noise3D>(L, static_cast<std::uint64_t>(luaL_optinteger(L, 1, 0)));
    return 1;
}

bool Noise3DBinding::hasW(lua_State* L) {
    return lua_type(L, 5) == LUA_TNUMBER;
}

haylen::math::Vec3 Noise3DBinding::readPoint(lua_State* L) {
    return {lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)};
}

int Noise3DBinding::perlin(lua_State* L) {
    const haylen::math::Noise3D& noise = lua::Userdata::check<haylen::math::Noise3D>(L, 1);
    const haylen::math::Vec3 point = readPoint(L);
    lua::Stack::push(L, hasW(L) ? noise.perlin(haylen::math::Vec4::fromVec3(point, lua::Stack::read<float>(L, 5))) : noise.perlin(point));
    return 1;
}

int Noise3DBinding::simplex(lua_State* L) {
    const haylen::math::Noise3D& noise = lua::Userdata::check<haylen::math::Noise3D>(L, 1);
    const haylen::math::Vec3 point = readPoint(L);
    lua::Stack::push(L, hasW(L) ? noise.simplex(haylen::math::Vec4::fromVec3(point, lua::Stack::read<float>(L, 5))) : noise.simplex(point));
    return 1;
}

int Noise3DBinding::value(lua_State* L) {
    const haylen::math::Noise3D& noise = lua::Userdata::check<haylen::math::Noise3D>(L, 1);
    const haylen::math::Vec3 point = readPoint(L);
    lua::Stack::push(L, hasW(L) ? noise.value(haylen::math::Vec4::fromVec3(point, lua::Stack::read<float>(L, 5))) : noise.value(point));
    return 1;
}

int Noise3DBinding::cellular(lua_State* L) {
    const haylen::math::Noise3D& noise = lua::Userdata::check<haylen::math::Noise3D>(L, 1);
    const haylen::math::Vec3 point = readPoint(L);
    const haylen::math::Noise3D::Cellular result = hasW(L) ? noise.cellular(haylen::math::Vec4::fromVec3(point, lua::Stack::read<float>(L, 5))) : noise.cellular(point);
    lua::Stack::push(L, result.nearest);
    lua::Stack::push(L, result.second);
    lua::Stack::push(L, result.cell);
    return 3;
}

// Reads `{basis = 'simplex', fractal = 'fbm', octaves = 4, lacunarity = 2, gain = 0.5, pingPongStrength = 2}`.
haylen::math::Noise3D::FractalOptions Noise3DBinding::readFractal(lua_State* L, int index) {
    haylen::math::Noise3D::FractalOptions options;
    if (lua_isnoneornil(L, index)) {
        return options;
    }

    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kFractalFields});
    lua::Table::readField(L, index, "basis", options.basis);
    lua::Table::readField(L, index, "fractal", options.fractal);
    lua::Table::readField(L, index, "octaves", options.octaves);
    lua::Table::readField(L, index, "lacunarity", options.lacunarity);
    lua::Table::readField(L, index, "gain", options.gain);
    lua::Table::readField(L, index, "pingPongStrength", options.pingPongStrength);
    return options;
}

// Takes `fractal(x, y, z, options)` or `fractal(x, y, z, w, options)`.
int Noise3DBinding::fractal(lua_State* L) {
    const haylen::math::Noise3D& noise = lua::Userdata::check<haylen::math::Noise3D>(L, 1);
    const haylen::math::Vec3 point = readPoint(L);
    if (hasW(L)) {
        const haylen::math::Vec4 point4 = haylen::math::Vec4::fromVec3(point, lua::Stack::read<float>(L, 5));
        lua::Stack::push(L, noise.fractal(point4, readFractal(L, 6)));
        return 1;
    }

    lua::Stack::push(L, noise.fractal(point, readFractal(L, 5)));
    return 1;
}

// Takes `warp(x, y, z, amplitude, frequency, octaves)` and returns the moved point as three numbers.
int Noise3DBinding::warp(lua_State* L) {
    const haylen::math::Noise3D& noise = lua::Userdata::check<haylen::math::Noise3D>(L, 1);
    const haylen::math::Vec3 point = readPoint(L);
    const haylen::math::Vec3 warped = noise.warp(point, lua::Stack::read<float>(L, 5), lua::Converter<float>::readOr(L, 6, 1.0), lua::Stack::read<std::optional<int>>(L, 7).value_or(3));
    lua::Stack::push(L, warped.x);
    lua::Stack::push(L, warped.y);
    lua::Stack::push(L, warped.z);
    return 3;
}

void Noise3DBinding::install(lua_State* L) {
    lua::ClassBuilder<haylen::math::Noise3D>(L).function("perlin", &lua::Binding::native<&perlin>).function("simplex", &lua::Binding::native<&simplex>).function("value", &lua::Binding::native<&value>).function("cellular", &lua::Binding::native<&cellular>).function("fractal", &lua::Binding::native<&fractal>).function("warp", &lua::Binding::native<&warp>).install();
}

void Noise3DBinding::addFunctions(lua_State* L) {
    lua_pushcfunction(L, &lua::Binding::native<&newNoise>);
    lua_setfield(L, -2, "noise3d");
}

} // namespace haylen::lua::math
