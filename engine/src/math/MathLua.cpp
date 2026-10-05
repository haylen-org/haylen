#include "math/MathLua.hpp"

#include <algorithm>

#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/math/EasingCurve.hpp"
#include "haylen/math/Geometry.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/PoissonDisk.hpp"
#include "math/ChoiceLua.hpp"
#include "math/PolygonLua.hpp"
#include "math/RaycastLua.hpp"
#include "math/SplineLua.hpp"
#include "math/SpringLua.hpp"

namespace haylen::math {

// Constructors take their components as numbers, or one whole value in any form that parameters of the type accept.
bool MathLua::isWholeValue(lua_State* L) {
    return !lua_isnoneornil(L, 1) && lua_type(L, 1) != LUA_TNUMBER;
}

int MathLua::vec2New(lua_State* L) {
    if (isWholeValue(L)) {
        lua::Stack::push(L, lua::Stack::read<Vec2>(L, 1));
        return 1;
    }
    lua::Stack::push(L, Vec2{static_cast<float>(luaL_optnumber(L, 1, 0.0)), static_cast<float>(luaL_optnumber(L, 2, 0.0))});
    return 1;
}

Vec2 MathLua::vec2FromAngle(float radians, std::optional<float> magnitude) {
    return Vec2::fromAngle(radians, magnitude.value_or(1.0F));
}

int MathLua::vec2Add(lua_State* L) {
    lua::Stack::push(L, lua::Stack::read<Vec2>(L, 1) + lua::Stack::read<Vec2>(L, 2));
    return 1;
}

int MathLua::vec2Subtract(lua_State* L) {
    lua::Stack::push(L, lua::Stack::read<Vec2>(L, 1) - lua::Stack::read<Vec2>(L, 2));
    return 1;
}

int MathLua::vec2Multiply(lua_State* L) {
    if (lua_type(L, 1) == LUA_TNUMBER) {
        lua::Stack::push(L, lua::Stack::read<Vec2>(L, 2) * static_cast<float>(lua_tonumber(L, 1)));
    } else if (lua_type(L, 2) == LUA_TNUMBER) {
        lua::Stack::push(L, lua::Stack::read<Vec2>(L, 1) * static_cast<float>(lua_tonumber(L, 2)));
    } else {
        lua::Stack::push(L, lua::Stack::read<Vec2>(L, 1) * lua::Stack::read<Vec2>(L, 2));
    }
    return 1;
}

int MathLua::vec2Divide(lua_State* L) {
    if (lua_type(L, 2) == LUA_TNUMBER) {
        lua::Stack::push(L, lua::Stack::read<Vec2>(L, 1) / static_cast<float>(lua_tonumber(L, 2)));
    } else {
        lua::Stack::push(L, lua::Stack::read<Vec2>(L, 1) / lua::Stack::read<Vec2>(L, 2));
    }
    return 1;
}

int MathLua::vec2Negate(lua_State* L) {
    lua::Stack::push(L, -lua::Stack::read<Vec2>(L, 1));
    return 1;
}

int MathLua::vec2Equal(lua_State* L) {
    lua_pushboolean(L, lua::Stack::read<Vec2>(L, 1) == lua::Stack::read<Vec2>(L, 2) ? 1 : 0);
    return 1;
}

int MathLua::vec2ToString(lua_State* L) {
    const Vec2 value = lua::Stack::read<Vec2>(L, 1);
    lua_pushfstring(L, "Vec2(%f, %f)", static_cast<double>(value.x), static_cast<double>(value.y));
    return 1;
}

int MathLua::vec2Unpack(lua_State* L) {
    const Vec2 value = lua::Stack::read<Vec2>(L, 1);
    lua_pushnumber(L, value.x);
    lua_pushnumber(L, value.y);
    return 2;
}

Vec2 MathLua::vec2Normalized(const Vec2& value) {
    return value.getNormalized();
}

int MathLua::rectNew(lua_State* L) {
    if (isWholeValue(L)) {
        lua::Stack::push(L, lua::Stack::read<Rect>(L, 1));
        return 1;
    }
    lua::Stack::push(L, Rect{static_cast<float>(luaL_optnumber(L, 1, 0.0)), static_cast<float>(luaL_optnumber(L, 2, 0.0)), static_cast<float>(luaL_optnumber(L, 3, 0.0)), static_cast<float>(luaL_optnumber(L, 4, 0.0))});
    return 1;
}

int MathLua::rectEqual(lua_State* L) {
    lua_pushboolean(L, lua::Stack::read<Rect>(L, 1) == lua::Stack::read<Rect>(L, 2) ? 1 : 0);
    return 1;
}

int MathLua::rectToString(lua_State* L) {
    const Rect value = lua::Stack::read<Rect>(L, 1);
    lua_pushfstring(L, "Rect(%f, %f, %f, %f)", static_cast<double>(value.x), static_cast<double>(value.y), static_cast<double>(value.width), static_cast<double>(value.height));
    return 1;
}

// Accepts either a point or a rectangle, telling tables apart by their `width` field.
int MathLua::rectContains(lua_State* L) {
    const Rect& self = lua::Userdata::check<Rect>(L, 1);
    bool rectangle = lua::Userdata::test<Rect>(L, 2) != nullptr;
    if (!rectangle && lua::Userdata::test<Vec2>(L, 2) == nullptr) {
        luaL_checktype(L, 2, LUA_TTABLE);
        rectangle = lua_getfield(L, 2, "width") != LUA_TNIL;
        lua_pop(L, 1);
    }
    lua_pushboolean(L, (rectangle ? self.contains(lua::Stack::read<Rect>(L, 2)) : self.contains(lua::Stack::read<Vec2>(L, 2))) ? 1 : 0);
    return 1;
}

bool MathLua::rectIntersects(const Rect& self, Rect other) {
    return self.intersects(other);
}

Rect MathLua::rectIntersection(const Rect& self, Rect other) {
    return self.intersection(other);
}

Rect MathLua::rectMerged(const Rect& self, Rect other) {
    return self.merged(other);
}

Rect MathLua::rectExpanded(const Rect& self, float amount) {
    return self.expanded(amount);
}

int MathLua::colorNew(lua_State* L) {
    if (isWholeValue(L)) {
        lua::Stack::push(L, lua::Stack::read<Color>(L, 1));
        return 1;
    }
    lua::Stack::push(L, Color{static_cast<float>(luaL_optnumber(L, 1, 1.0)), static_cast<float>(luaL_optnumber(L, 2, 1.0)), static_cast<float>(luaL_optnumber(L, 3, 1.0)), static_cast<float>(luaL_optnumber(L, 4, 1.0))});
    return 1;
}

int MathLua::colorEqual(lua_State* L) {
    lua_pushboolean(L, lua::Stack::read<Color>(L, 1) == lua::Stack::read<Color>(L, 2) ? 1 : 0);
    return 1;
}

int MathLua::colorMultiply(lua_State* L) {
    lua::Stack::push(L, lua::Stack::read<Color>(L, 1) * lua::Stack::read<Color>(L, 2));
    return 1;
}

int MathLua::colorToHex(lua_State* L) {
    lua::Stack::push(L, lua::Stack::read<Color>(L, 1).toHex());
    return 1;
}

Color MathLua::colorWithAlpha(const Color& self, float alpha) {
    return self.withAlpha(alpha);
}

Color MathLua::colorFromHsv(float hue, float saturation, float value, std::optional<float> alpha) {
    return Color::fromHsv(hue, saturation, value, alpha.value_or(1.0F));
}

int MathLua::colorToHsv(lua_State* L) {
    const Color::Hsv hsv = lua::Userdata::check<Color>(L, 1).toHsv();
    lua::Stack::push(L, hsv.hue);
    lua::Stack::push(L, hsv.saturation);
    lua::Stack::push(L, hsv.value);
    lua::Stack::push(L, hsv.alpha);
    return 4;
}

float MathLua::ease(const EasingCurve& curve, float t) {
    return curve.apply(t);
}

// An `EasingCurve` value is called like the function it stands for, with the progress after the value itself.
int MathLua::curveCall(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<EasingCurve>(L, 1).apply(lua::Stack::read<float>(L, 2)));
    return 1;
}

std::uint8_t MathLua::checkChannel(lua_State* L, int index) {
    const lua_Integer value = luaL_checkinteger(L, index);
    luaL_argcheck(L, value >= 0 && value <= 255, index, "expected an integer from 0 to 255");
    return static_cast<std::uint8_t>(value);
}

int MathLua::colorFromRgba8(lua_State* L) {
    const std::uint8_t red = checkChannel(L, 1);
    const std::uint8_t green = checkChannel(L, 2);
    const std::uint8_t blue = checkChannel(L, 3);
    const std::uint8_t alpha = lua_isnoneornil(L, 4) ? 255 : checkChannel(L, 4);
    lua::Stack::push(L, Color::fromRgba8(red, green, blue, alpha));
    return 1;
}

int MathLua::colorFromHex(lua_State* L) {
    const lua_Integer value = luaL_checkinteger(L, 1);
    luaL_argcheck(L, value >= 0 && value <= 0xFFFFFFFF, 1, "expected an integer from 0 to 0xFFFFFFFF");
    lua::Stack::push(L, Color::fromHex(static_cast<std::uint32_t>(value)));
    return 1;
}

Transform2D MathLua::transformCompose(Vec2 position, std::optional<float> rotation, std::optional<Vec2> scale, std::optional<Vec2> skew) {
    return Transform2D::compose(position, rotation.value_or(0.0F), scale.value_or(Vec2{1.0F, 1.0F}), skew.value_or(Vec2{}));
}

int MathLua::transformMultiply(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Transform2D>(L, 1) * lua::Userdata::check<Transform2D>(L, 2));
    return 1;
}

Vec2 MathLua::transformApply(const Transform2D& self, Vec2 point) {
    return self.apply(point);
}

Vec2 MathLua::transformApplyVector(const Transform2D& self, Vec2 vector) {
    return self.applyVector(vector);
}

Transform2D MathLua::transformInverse(const Transform2D& self) {
    return self.getInverse();
}

int MathLua::randomNew(lua_State* L) {
    if (lua_isnoneornil(L, 1)) {
        lua::Userdata::emplace<Random>(L);
        return 1;
    }
    lua::Userdata::emplace<Random>(L, static_cast<std::uint64_t>(luaL_checkinteger(L, 1)));
    return 1;
}

float MathLua::randomNextFloat(Random& self) {
    return self.nextFloat();
}

float MathLua::randomRange(Random& self, float minimum, float maximum) {
    return self.range(minimum, maximum);
}

int MathLua::randomInteger(Random& self, int minimum, int maximum) {
    return self.range(minimum, maximum);
}

bool MathLua::randomChance(Random& self, float probability) {
    return self.chance(probability);
}

void MathLua::randomReseed(Random& self, lua_Integer seed) {
    self.reseed(static_cast<std::uint64_t>(seed));
}

int MathLua::randomWeightedIndex(lua_State* L) {
    Random& self = lua::Userdata::check<Random>(L, 1);
    const std::vector<float> weights = lua::Stack::read<std::vector<float>>(L, 2);
    if (weights.empty()) {
        return luaL_argerror(L, 2, "expected at least one weight");
    }
    lua_pushinteger(L, static_cast<lua_Integer>(self.weightedIndex(weights) + 1));
    return 1;
}

int MathLua::randomShuffle(lua_State* L) {
    Random& self = lua::Userdata::check<Random>(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    const lua_Integer length = luaL_len(L, 2);
    for (lua_Integer index = length; index > 1; --index) {
        const auto other = static_cast<lua_Integer>(self.nextU64() % static_cast<std::uint64_t>(index)) + 1;
        lua_rawgeti(L, 2, index);
        lua_rawgeti(L, 2, other);
        lua_rawseti(L, 2, index);
        lua_rawseti(L, 2, other);
    }
    lua_settop(L, 2);
    return 1;
}

int MathLua::noiseNew(lua_State* L) {
    lua::Userdata::emplace<Noise2D>(L, static_cast<std::uint64_t>(luaL_optinteger(L, 1, 0)));
    return 1;
}

float MathLua::noisePerlin(Noise2D& self, float x, float y) {
    return self.perlin(x, y);
}

float MathLua::noiseSimplex(Noise2D& self, float x, float y) {
    return self.simplex(x, y);
}

float MathLua::noiseFractal(Noise2D& self, float x, float y, std::optional<int> octaves, std::optional<float> lacunarity, std::optional<float> gain) {
    return self.fractal(x, y, octaves.value_or(4), lacunarity.value_or(2.0F), gain.value_or(0.5F));
}

int MathLua::noiseWorley(lua_State* L) {
    const Noise2D::Cellular cellular = lua::Userdata::check<Noise2D>(L, 1).worley(lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3));
    lua::Stack::push(L, cellular.nearest);
    lua::Stack::push(L, cellular.second);
    lua::Stack::push(L, cellular.cell);
    return 3;
}

int MathLua::noiseWarp(lua_State* L) {
    const Noise2D& noise = lua::Userdata::check<Noise2D>(L, 1);
    const Vec2 warped = noise.warp(lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4), static_cast<float>(luaL_optnumber(L, 5, 1.0)), static_cast<int>(luaL_optinteger(L, 6, 3)));
    lua::Stack::push(L, warped.x);
    lua::Stack::push(L, warped.y);
    return 2;
}

int MathLua::clampValue(lua_State* L) {
    const auto value = lua::Stack::read<float>(L, 1);
    const auto minimum = lua::Stack::read<float>(L, 2);
    const auto maximum = lua::Stack::read<float>(L, 3);
    luaL_argcheck(L, minimum <= maximum, 3, "expected a maximum of at least the minimum");
    lua::Stack::push(L, std::clamp(value, minimum, maximum));
    return 1;
}

bool MathLua::approximatelyValue(float lhs, float rhs, std::optional<float> epsilon) {
    return Math::approximately(lhs, rhs, epsilon.value_or(1e-5F));
}

int MathLua::circleIntersects(lua_State* L) {
    const auto circle = lua::Stack::read<Circle>(L, 1);
    lua::Stack::push(L, lua::Stack::is<Circle>(L, 2) ? Geometry::intersects(circle, lua::Stack::read<Circle>(L, 2)) : Geometry::intersects(circle, lua::Stack::read<Rect>(L, 2)));
    return 1;
}

Rect MathLua::pointBounds(std::vector<Vec2> points) {
    return Geometry::bounds(points);
}

bool MathLua::polygonContains(std::vector<Vec2> polygon, Vec2 point) {
    return Geometry::contains(polygon, point);
}

float MathLua::polygonSignedArea(std::vector<Vec2> polygon) {
    return Geometry::signedArea(polygon);
}

Vec2 MathLua::polygonCentroid(std::vector<Vec2> polygon) {
    return Geometry::centroid(polygon);
}

bool MathLua::polygonConvex(std::vector<Vec2> polygon) {
    return Geometry::isConvex(polygon);
}

std::vector<Vec2> MathLua::hull(std::vector<Vec2> points) {
    return Geometry::convexHull(points);
}

int MathLua::triangulatePolygon(lua_State* L) {
    const std::vector<std::uint32_t> indices = Geometry::triangulate(lua::Stack::read<std::vector<Vec2>>(L, 1));
    lua_createtable(L, static_cast<int>(indices.size()), 0);
    for (std::size_t index = 0; index < indices.size(); ++index) {
        lua_pushinteger(L, static_cast<lua_Integer>(indices[index] + 1));
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    return 1;
}

// Samples points with `poissonDisk({area = rect, minimumDistance = 64, maximumDistance = 128, attempts = 30, random = rng or seed = n, accept = function(point), distance = function(point)})`.
int MathLua::poissonDisk(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kPoissonFields});
    PoissonDisk::Options options;
    lua::Table::readField(L, 1, "area", options.area);
    lua::Table::readField(L, 1, "minimumDistance", options.minimumDistance);
    lua::Table::readField(L, 1, "maximumDistance", options.maximumDistance);
    lua::Table::readField(L, 1, "attempts", options.attempts);

    // The generator stays on the stack while sampling, so an `accept` function that drops it from the options cannot free it.
    Random local(0);
    Random* random = &local;
    lua_getfield(L, 1, "random");
    if (!lua_isnil(L, -1)) {
        random = &lua::Userdata::check<Random>(L, -1);
    }
    lua_Integer seed = 0;
    lua::Table::readField(L, 1, "seed", seed);
    local.reseed(static_cast<std::uint64_t>(seed));

    lua_getfield(L, 1, "accept");
    const bool hasPredicate = lua_isfunction(L, -1);
    const int predicate = lua_gettop(L);
    if (hasPredicate) {
        // clang-format off
        options.accept = [L, predicate](Vec2 point) {
            lua_pushvalue(L, predicate);
            lua::Stack::push(L, point);
            lua::Runtime::protectedCall(L, 1, 1);
            const bool accepted = lua_toboolean(L, -1) != 0;
            lua_pop(L, 1);
            return accepted;
        };
        // clang-format on
    }

    lua_getfield(L, 1, "distance");
    const int spacing = lua_gettop(L);
    if (lua_isfunction(L, spacing)) {
        // clang-format off
        options.distance = [L, spacing](Vec2 point) {
            lua_pushvalue(L, spacing);
            lua::Stack::push(L, point);
            lua::Runtime::protectedCall(L, 1, 1);
            const auto distance = static_cast<float>(luaL_checknumber(L, -1));
            lua_pop(L, 1);
            return distance;
        };
        // clang-format on
    }

    lua::Stack::push(L, PoissonDisk::sample(options, *random));
    return 1;
}

void MathLua::installClasses(lua_State* L) {
    lua::ClassBuilder<Vec2>(L).field<&Vec2::x>("x").field<&Vec2::y>("y").method<&Vec2::getLength>("length").method<&Vec2::getLengthSquared>("lengthSquared").method<&Vec2::getAngle>("angle").method<&Vec2::rotated>("rotated").method<&Vec2::getPerpendicular>("perpendicular").method<&Vec2::clampedLength>("clampedLength").method<&Vec2::isZero>("isZero").function("normalized", &lua::Binding::function<&vec2Normalized>).function("dot", &lua::Binding::function<&Vec2::dot>).function("cross", &lua::Binding::function<&Vec2::cross>).function("distance", &lua::Binding::function<&Vec2::distance>).function("distanceSquared", &lua::Binding::function<&Vec2::distanceSquared>).function("lerp", &lua::Binding::function<&Vec2::lerp>).function("min", &lua::Binding::function<&Vec2::min>).function("max", &lua::Binding::function<&Vec2::max>).function("floor", &lua::Binding::function<&Vec2::floor>).function("round", &lua::Binding::function<&Vec2::round>).function("unpack", &vec2Unpack).meta("__add", &vec2Add).meta("__sub", &vec2Subtract).meta("__mul", &vec2Multiply).meta("__div", &vec2Divide).meta("__unm", &vec2Negate).meta("__eq", &vec2Equal).meta("__tostring", &vec2ToString).install();

    lua::ClassBuilder<Rect>(L).field<&Rect::x>("x").field<&Rect::y>("y").field<&Rect::width>("width").field<&Rect::height>("height").method<&Rect::getLeft>("left").method<&Rect::getRight>("right").method<&Rect::getTop>("top").method<&Rect::getBottom>("bottom").method<&Rect::getPosition>("position").method<&Rect::getCenter>("center").method<&Rect::getSize>("size").method<&Rect::getMin>("min").method<&Rect::getMax>("max").method<&Rect::getArea>("area").method<&Rect::isEmpty>("empty").function("contains", &lua::Binding::native<&rectContains>).function("intersects", &lua::Binding::function<&rectIntersects>).function("intersection", &lua::Binding::function<&rectIntersection>).function("merged", &lua::Binding::function<&rectMerged>).function("expanded", &lua::Binding::function<&rectExpanded>).method<&Rect::inset>("inset").method<&Rect::translated>("translated").method<&Rect::clamp>("clamp").meta("__eq", &rectEqual).meta("__tostring", &rectToString).install();

    lua::ClassBuilder<Color>(L).field<&Color::r>("r").field<&Color::g>("g").field<&Color::b>("b").field<&Color::a>("a").function("withAlpha", &lua::Binding::function<&colorWithAlpha>).method<&Color::getPremultiplied>("premultiplied").function("lerp", &lua::Binding::function<&Color::lerp>).function("lerpHsv", &lua::Binding::function<&Color::lerpHsv>).function("toHsv", &colorToHsv).function("toHex", &colorToHex).meta("__eq", &colorEqual).meta("__mul", &colorMultiply).meta("__tostring", &colorToHex).install();

    lua::ClassBuilder<Transform2D>(L).field<&Transform2D::a>("a").field<&Transform2D::b>("b").field<&Transform2D::c>("c").field<&Transform2D::d>("d").field<&Transform2D::tx>("tx").field<&Transform2D::ty>("ty").function("apply", &lua::Binding::function<&transformApply>).function("applyVector", &lua::Binding::function<&transformApplyVector>).method<&Transform2D::getDeterminant>("determinant").method<&Transform2D::getTranslation>("translation").function("inverse", &lua::Binding::function<&transformInverse>).meta("__mul", &transformMultiply).install();

    lua::ClassBuilder<Random>(L).function("nextFloat", &lua::Binding::function<&randomNextFloat>).function("range", &lua::Binding::function<&randomRange>).function("integer", &lua::Binding::function<&randomInteger>).function("chance", &lua::Binding::function<&randomChance>).function("reseed", &lua::Binding::function<&randomReseed>).function("weightedIndex", &lua::Binding::native<&randomWeightedIndex>).function("shuffle", &randomShuffle).install();

    lua::ClassBuilder<Noise2D>(L).function("perlin", &lua::Binding::function<&noisePerlin>).function("simplex", &lua::Binding::function<&noiseSimplex>).function("fractal", &lua::Binding::function<&noiseFractal>).function("worley", &lua::Binding::native<&noiseWorley>).function("warp", &lua::Binding::native<&noiseWarp>).install();

    lua::ClassBuilder<EasingCurve>(L).meta("__call", &lua::Binding::native<&curveCall>).install();

    SplineLua::install(L);
    SpringLua::install(L);
    ChoiceLua::install(L);
}

int MathLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"vec2", &vec2New}, {"fromAngle", &lua::Binding::function<&vec2FromAngle>}, {"rect", &rectNew}, {"fromMinMax", &lua::Binding::function<&Rect::fromMinMax>}, {"fromCenter", &lua::Binding::function<&Rect::fromCenter>}, {"color", &lua::Binding::native<&colorNew>}, {"white", &lua::Binding::function<&Color::white>}, {"black", &lua::Binding::function<&Color::black>}, {"transparent", &lua::Binding::function<&Color::transparent>}, {"fromRgba8", &colorFromRgba8}, {"fromHex", &colorFromHex}, {"fromHsv", &lua::Binding::function<&colorFromHsv>}, {"transform", &lua::Binding::function<&transformCompose>}, {"identity", &lua::Binding::function<&Transform2D::identity>}, {"translation", &lua::Binding::function<&Transform2D::translation>}, {"rotation", &lua::Binding::function<&Transform2D::rotation>}, {"scaling", &lua::Binding::function<&Transform2D::scaling>}, {"random", &randomNew}, {"noise", &noiseNew}, {"ease", &lua::Binding::function<&ease>}, {"clamp", &clampValue}, {"lerp", &lua::Binding::function<&Math::lerp>}, {"inverseLerp", &lua::Binding::function<&Math::inverseLerp>}, {"remap", &lua::Binding::function<&Math::remap>}, {"smoothstep", &lua::Binding::function<&Math::smoothstep>}, {"moveToward", &lua::Binding::function<&Math::moveToward>}, {"wrapAngle", &lua::Binding::function<&Math::wrapAngle>}, {"dampFactor", &lua::Binding::function<&Math::dampFactor>}, {"radians", &lua::Binding::function<&Math::radians>}, {"degrees", &lua::Binding::function<&Math::degrees>}, {"approximately", &lua::Binding::function<&approximatelyValue>}, {"sign", &lua::Binding::function<&Math::sign>}, {"saturate", &lua::Binding::function<&Math::saturate>}, {"intersects", &circleIntersects}, {"intersection", &lua::Binding::function<&Geometry::intersection>}, {"closestPoint", &lua::Binding::function<&Geometry::closestPoint>}, {"distanceToSegment", &lua::Binding::function<&Geometry::distanceToSegment>}, {"bounds", &lua::Binding::function<&pointBounds>}, {"polygonContains", &lua::Binding::function<&polygonContains>}, {"polygonSignedArea", &lua::Binding::function<&polygonSignedArea>}, {"polygonCentroid", &lua::Binding::function<&polygonCentroid>}, {"polygonConvex", &lua::Binding::function<&polygonConvex>}, {"convexHull", &lua::Binding::function<&hull>}, {"triangulate", &lua::Binding::native<&triangulatePolygon>}, {"poissonDisk", &lua::Binding::native<&poissonDisk>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    RaycastLua::addFunctions(L);
    PolygonLua::addFunctions(L);
    SplineLua::addFunctions(L);
    SpringLua::addFunctions(L);
    ChoiceLua::addFunctions(L);
    lua_pushnumber(L, Math::kPi);
    lua_setfield(L, -2, "pi");
    lua_pushnumber(L, Math::kTau);
    lua_setfield(L, -2, "tau");
    lua_pushnumber(L, Math::kHalfPi);
    lua_setfield(L, -2, "halfPi");
    return 1;
}

void MathLua::install(lua_State* L) {
    installClasses(L);
    lua::Binding::preload(L, "haylen.math", &open);
}

} // namespace haylen::math
