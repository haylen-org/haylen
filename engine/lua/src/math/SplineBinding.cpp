#include "math/SplineBinding.hpp"

#include <optional>
#include <vector>

#include "haylen/core/EnumNames.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/math/Spline.hpp"

namespace haylen::core {

template <> struct EnumNames<haylen::math::Spline::Kind> {
    static std::optional<haylen::math::Spline::Kind> fromName(std::string_view name) {
        return haylen::math::Spline::kindFromName(name);
    }
    static std::string_view name(haylen::math::Spline::Kind value) {
        return haylen::math::Spline::kindName(value);
    }
};

} // namespace haylen::core

namespace haylen::lua::math {

// Creates a spline with `spline(points, {kind = 'catmullRom', closed = false})`.
int SplineBinding::newSpline(lua_State* L) {
    auto points = lua::Stack::read<std::vector<haylen::math::Vec2>>(L, 1);
    haylen::math::Spline::Kind kind = haylen::math::Spline::Kind::CatmullRom;
    bool closed = false;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kSplineFields});
        lua::Table::readField(L, 2, "kind", kind);
        lua::Table::readField(L, 2, "closed", closed);
    }

    lua::Userdata::emplace<haylen::math::Spline>(L, std::move(points), kind, closed);
    return 1;
}

int SplineBinding::point(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline>(L, 1).getPoint(lua::Stack::read<float>(L, 2)));
    return 1;
}

int SplineBinding::tangent(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline>(L, 1).getTangent(lua::Stack::read<float>(L, 2)));
    return 1;
}

int SplineBinding::pointAtDistance(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline>(L, 1).getPointAtDistance(lua::Stack::read<float>(L, 2)));
    return 1;
}

int SplineBinding::tangentAtDistance(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline>(L, 1).getTangentAtDistance(lua::Stack::read<float>(L, 2)));
    return 1;
}

int SplineBinding::parameterAtDistance(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline>(L, 1).getParameterAtDistance(lua::Stack::read<float>(L, 2)));
    return 1;
}

int SplineBinding::sample(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline>(L, 1).sample(lua::Stack::read<std::size_t>(L, 2)));
    return 1;
}

int SplineBinding::flatten(lua_State* L) {
    std::vector<haylen::math::Vec2> flattened;
    lua::Userdata::check<haylen::math::Spline>(L, 1).flatten(lua::Stack::read<float>(L, 2), flattened);
    lua::Stack::push(L, flattened);
    return 1;
}

int SplineBinding::sampleByDistance(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline>(L, 1).sampleByDistance(lua::Stack::read<float>(L, 2)));
    return 1;
}

int SplineBinding::getLength(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline>(L, 1).getLength());
    return 1;
}

int SplineBinding::getKind(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline>(L, 1).getKind());
    return 1;
}

int SplineBinding::isClosed(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline>(L, 1).isClosed());
    return 1;
}

int SplineBinding::getPoints(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline>(L, 1).getPoints());
    return 1;
}

int SplineBinding::getSegmentCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Spline>(L, 1).getSegmentCount());
    return 1;
}

void SplineBinding::install(lua_State* L) {
    lua::ClassBuilder<haylen::math::Spline>(L).function("point", &lua::Binding::native<&point>).function("tangent", &lua::Binding::native<&tangent>).function("pointAtDistance", &lua::Binding::native<&pointAtDistance>).function("tangentAtDistance", &lua::Binding::native<&tangentAtDistance>).function("parameterAtDistance", &lua::Binding::native<&parameterAtDistance>).function("sample", &lua::Binding::native<&sample>).function("sampleByDistance", &lua::Binding::native<&sampleByDistance>).function("flatten", &lua::Binding::native<&flatten>).property("length", &getLength).property("kind", &getKind).property("closed", &isClosed).property("points", &getPoints).property("segmentCount", &getSegmentCount).install();
}

void SplineBinding::addFunctions(lua_State* L) {
    lua_pushcfunction(L, &lua::Binding::native<&newSpline>);
    lua_setfield(L, -2, "spline");
}

} // namespace haylen::lua::math
