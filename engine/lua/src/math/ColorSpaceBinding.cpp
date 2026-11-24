#include "math/ColorSpaceBinding.hpp"

#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/math/ColorSpace.hpp"
#include "haylen/math/Photometry.hpp"

namespace haylen::lua::math {

// Takes a color of linear light, the space of the screen and `{referenceWhite = 203}`.
int ColorSpaceBinding::encode(lua_State* L) {
    const auto linear = lua::Stack::read<haylen::math::Color>(L, 1);
    const auto output = lua::Stack::read<haylen::math::ColorSpace::Kind>(L, 2);
    lua::Stack::push(L, haylen::math::ColorSpace::encode(linear, output, readReferenceWhite(L, 3)));
    return 1;
}

int ColorSpaceBinding::decode(lua_State* L) {
    const auto signal = lua::Stack::read<haylen::math::Color>(L, 1);
    const auto output = lua::Stack::read<haylen::math::ColorSpace::Kind>(L, 2);
    lua::Stack::push(L, haylen::math::ColorSpace::decode(signal, output, readReferenceWhite(L, 3)));
    return 1;
}

float ColorSpaceBinding::readReferenceWhite(lua_State* L, int index) {
    float referenceWhite = haylen::math::ColorSpace::kReferenceWhite;
    if (lua_isnoneornil(L, index)) {
        return referenceWhite;
    }

    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kSignalFields});
    lua::Table::readField(L, index, "referenceWhite", referenceWhite);
    return referenceWhite;
}

void ColorSpaceBinding::addFunctions(lua_State* L) {
    const luaL_Reg colorSpace[] = {
        {"encode", &lua::Binding::native<&encode>}, {"decode", &lua::Binding::native<&decode>}, {"convert", &lua::Binding::function<&haylen::math::ColorSpace::convert>}, {"toXyz", &lua::Binding::function<&haylen::math::ColorSpace::toXyz>}, {"fromXyz", &lua::Binding::function<&haylen::math::ColorSpace::fromXyz>}, {"luminance", &lua::Binding::function<&haylen::math::ColorSpace::luminance>}, {"encodeSrgb", &lua::Binding::function<&haylen::math::ColorSpace::encodeSrgb>}, {"decodeSrgb", &lua::Binding::function<&haylen::math::ColorSpace::decodeSrgb>}, {"encodePq", &lua::Binding::function<&haylen::math::ColorSpace::encodePq>}, {"decodePq", &lua::Binding::function<&haylen::math::ColorSpace::decodePq>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, colorSpace);
    lua_pushnumber(L, haylen::math::ColorSpace::kReferenceWhite);
    lua_setfield(L, -2, "referenceWhite");
    lua_pushnumber(L, haylen::math::ColorSpace::kPeakNits);
    lua_setfield(L, -2, "peakNits");
    lua_setfield(L, -2, "colorSpace");

    const luaL_Reg photometry[] = {
        {"pointIntensity", &lua::Binding::function<&haylen::math::Photometry::pointIntensity>}, {"spotIntensity", &lua::Binding::function<&haylen::math::Photometry::spotIntensity>}, {"illuminance", &lua::Binding::function<&haylen::math::Photometry::illuminance>}, {"ev100FromCamera", &lua::Binding::function<&haylen::math::Photometry::ev100FromCamera>}, {"ev100FromLuminance", &lua::Binding::function<&haylen::math::Photometry::ev100FromLuminance>}, {"exposure", &lua::Binding::function<&haylen::math::Photometry::exposure>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, photometry);
    lua_setfield(L, -2, "photometry");
}

} // namespace haylen::lua::math
