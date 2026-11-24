#include "math/SphericalBinding.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <vector>

#include "core/FloatBufferBinding.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "math/NumberText.hpp"

namespace haylen::lua::math {

// Takes the radius, the polar angle and the azimuth as numbers, or a point in any form that a `Vec3` argument accepts, which it converts.
int SphericalBinding::newSpherical(lua_State* L) {
    if (!lua_isnoneornil(L, 1) && lua_type(L, 1) != LUA_TNUMBER) {
        lua::Stack::push(L, haylen::math::Spherical::fromVector(lua::Stack::read<haylen::math::Vec3>(L, 1)));
        return 1;
    }

    haylen::math::Spherical coordinates;
    if (!lua_isnoneornil(L, 1)) {
        coordinates = {.radius = lua::Stack::read<float>(L, 1), .polar = lua::Stack::read<float>(L, 2), .azimuth = lua::Stack::read<float>(L, 3)};
    }

    lua::Stack::push(L, coordinates);
    return 1;
}

// Takes the radius, the azimuth and the height as numbers, or a point in any form that a `Vec3` argument accepts, which it converts.
int SphericalBinding::newCylindrical(lua_State* L) {
    if (!lua_isnoneornil(L, 1) && lua_type(L, 1) != LUA_TNUMBER) {
        lua::Stack::push(L, haylen::math::Cylindrical::fromVector(lua::Stack::read<haylen::math::Vec3>(L, 1)));
        return 1;
    }

    haylen::math::Cylindrical coordinates;
    if (!lua_isnoneornil(L, 1)) {
        coordinates = {.radius = lua::Stack::read<float>(L, 1), .azimuth = lua::Stack::read<float>(L, 2), .height = lua::Stack::read<float>(L, 3)};
    }

    lua::Stack::push(L, coordinates);
    return 1;
}

// Takes nothing for harmonics without light, or a list of the nine coefficients as `Vec3` values of red, green and blue.
int SphericalBinding::newHarmonics(lua_State* L) {
    if (lua_isnoneornil(L, 1)) {
        lua::Stack::push(L, haylen::math::SphericalHarmonics{});
        return 1;
    }

    const auto values = lua::Stack::read<std::vector<haylen::math::Vec3>>(L, 1);
    if (values.size() != haylen::math::SphericalHarmonics::kCount) {
        return luaL_error(L, "Spherical harmonics take 9 coefficients, not %d.", static_cast<int>(values.size()));
    }

    std::array<haylen::math::Vec3, haylen::math::SphericalHarmonics::kCount> coefficients{};
    std::ranges::copy(values, coefficients.begin());
    lua::Stack::push(L, haylen::math::SphericalHarmonics(coefficients));
    return 1;
}

int SphericalBinding::harmonicsFromCubemap(lua_State* L) {
    const std::shared_ptr<haylen::core::FloatBuffer> buffer = lua::Userdata::checkShared<haylen::core::FloatBuffer>(L, 1);
    lua::Stack::push(L, haylen::math::SphericalHarmonics::fromCubemap(buffer->getValues(), lua::Stack::read<std::size_t>(L, 2)));
    return 1;
}

int SphericalBinding::harmonicsFromPanorama(lua_State* L) {
    const std::shared_ptr<haylen::core::FloatBuffer> buffer = lua::Userdata::checkShared<haylen::core::FloatBuffer>(L, 1);
    const auto width = lua::Stack::read<std::size_t>(L, 2);
    const auto height = lua::Stack::read<std::size_t>(L, 3);
    lua::Stack::push(L, haylen::math::SphericalHarmonics::fromPanorama(buffer->getValues(), width, height));
    return 1;
}

int SphericalBinding::sphericalToString(lua_State* L) {
    const haylen::math::Spherical& value = lua::Userdata::check<haylen::math::Spherical>(L, 1);
    lua_pushfstring(L, "Spherical(%f, %f, %f)", NumberText::plain(value.radius), NumberText::plain(value.polar), NumberText::plain(value.azimuth));
    return 1;
}

int SphericalBinding::cylindricalToString(lua_State* L) {
    const haylen::math::Cylindrical& value = lua::Userdata::check<haylen::math::Cylindrical>(L, 1);
    lua_pushfstring(L, "Cylindrical(%f, %f, %f)", NumberText::plain(value.radius), NumberText::plain(value.azimuth), NumberText::plain(value.height));
    return 1;
}

// Adds the light in place, so many samples gather into one set of harmonics without a new value for each.
int SphericalBinding::harmonicsProjectDirection(lua_State* L) {
    haylen::math::SphericalHarmonics& harmonics = lua::Userdata::check<haylen::math::SphericalHarmonics>(L, 1);
    const auto direction = lua::Stack::read<haylen::math::Vec3>(L, 2);
    const auto color = lua::Stack::read<haylen::math::Color>(L, 3);
    harmonics.projectDirection(direction, color, lua_isnoneornil(L, 4) ? 1.0F : lua::Stack::read<float>(L, 4));
    return 0;
}

int SphericalBinding::harmonicsCoefficients(lua_State* L) {
    const haylen::math::SphericalHarmonics& harmonics = lua::Userdata::check<haylen::math::SphericalHarmonics>(L, 1);
    lua::Stack::push(L, std::vector<haylen::math::Vec3>(harmonics.getCoefficients().begin(), harmonics.getCoefficients().end()));
    return 1;
}

int SphericalBinding::harmonicsAdd(lua_State* L) {
    lua::Stack::push(L, lua::Stack::read<haylen::math::SphericalHarmonics>(L, 1) + lua::Stack::read<haylen::math::SphericalHarmonics>(L, 2));
    return 1;
}

// Scales by a number on either side.
int SphericalBinding::harmonicsMultiply(lua_State* L) {
    const int harmonics = lua_type(L, 1) == LUA_TNUMBER ? 2 : 1;
    const int scale = harmonics == 1 ? 2 : 1;
    lua::Stack::push(L, lua::Stack::read<haylen::math::SphericalHarmonics>(L, harmonics) * lua::Stack::read<float>(L, scale));
    return 1;
}

int SphericalBinding::harmonicsEqual(lua_State* L) {
    lua_pushboolean(L, lua::Stack::read<haylen::math::SphericalHarmonics>(L, 1) == lua::Stack::read<haylen::math::SphericalHarmonics>(L, 2) ? 1 : 0);
    return 1;
}

int SphericalBinding::harmonicsToString(lua_State* L) {
    const haylen::math::Vec3 ambient = lua::Userdata::check<haylen::math::SphericalHarmonics>(L, 1).getCoefficients().front();
    lua_pushfstring(L, "SphericalHarmonics(%f, %f, %f, ...)", NumberText::plain(ambient.x), NumberText::plain(ambient.y), NumberText::plain(ambient.z));
    return 1;
}

haylen::math::Color SphericalBinding::harmonicsEvaluate(const haylen::math::SphericalHarmonics& self, haylen::math::Vec3 direction) {
    return self.evaluate(direction);
}

haylen::math::SphericalHarmonics SphericalBinding::harmonicsConvolveLambert(const haylen::math::SphericalHarmonics& self) {
    return self.convolveLambert();
}

haylen::math::SphericalHarmonics SphericalBinding::harmonicsLerp(const haylen::math::SphericalHarmonics& self, const haylen::math::SphericalHarmonics& to, float t) {
    return haylen::math::SphericalHarmonics::lerp(self, to, t);
}

void SphericalBinding::install(lua_State* L) {
    using haylen::math::Cylindrical;
    using haylen::math::Spherical;
    using haylen::math::SphericalHarmonics;

    lua::ClassBuilder<Spherical>(L).field<&Spherical::radius>("radius").field<&Spherical::polar>("polar").field<&Spherical::azimuth>("azimuth").method<&Spherical::toVector>("vector").meta("__tostring", &lua::Binding::native<&sphericalToString>).install();

    lua::ClassBuilder<Cylindrical>(L).field<&Cylindrical::radius>("radius").field<&Cylindrical::azimuth>("azimuth").field<&Cylindrical::height>("height").method<&Cylindrical::toVector>("vector").meta("__tostring", &lua::Binding::native<&cylindricalToString>).install();

    lua::ClassBuilder<SphericalHarmonics>(L).function("evaluate", &lua::Binding::function<&harmonicsEvaluate>).function("convolveLambert", &lua::Binding::function<&harmonicsConvolveLambert>).function("lerp", &lua::Binding::function<&harmonicsLerp>).function("projectDirection", &lua::Binding::native<&harmonicsProjectDirection>).property("coefficients", &harmonicsCoefficients).meta("__add", &lua::Binding::native<&harmonicsAdd>).meta("__mul", &lua::Binding::native<&harmonicsMultiply>).meta("__eq", &lua::Binding::native<&harmonicsEqual>).meta("__tostring", &lua::Binding::native<&harmonicsToString>).install();
}

void SphericalBinding::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"spherical", &lua::Binding::native<&newSpherical>}, {"cylindrical", &lua::Binding::native<&newCylindrical>}, {"sphericalHarmonics", &lua::Binding::native<&newHarmonics>}, {"sphericalHarmonicsFromCubemap", &lua::Binding::native<&harmonicsFromCubemap>}, {"sphericalHarmonicsFromPanorama", &lua::Binding::native<&harmonicsFromPanorama>}, {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

} // namespace haylen::lua::math
