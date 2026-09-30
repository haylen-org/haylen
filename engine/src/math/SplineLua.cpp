#include "math/SplineLua.hpp"

#include <optional>
#include <vector>

#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/EnumNames.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/math/Spline.hpp"

namespace haylen::lua {

template <> struct Type<math::Spline> {
    static constexpr const char* name = "haylen.Spline";
    using Storage = math::Spline;
};

template <> struct EnumNames<math::Spline::Kind> {
    static std::optional<math::Spline::Kind> fromName(std::string_view name) {
        return math::Spline::kindFromName(name);
    }
    static std::string_view name(math::Spline::Kind value) {
        return math::Spline::kindName(value);
    }
};

} // namespace haylen::lua

namespace haylen::math {

// Creates a spline with `spline(points, {kind = 'catmullRom', closed = false})`.
int SplineLua::newSpline(lua_State* L) {
    auto points = lua::Stack::read<std::vector<Vec2>>(L, 1);
    Spline::Kind kind = Spline::Kind::CatmullRom;
    bool closed = false;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kSplineFields});
        lua::Table::readField(L, 2, "kind", kind);
        lua::Table::readField(L, 2, "closed", closed);
    }
    lua::Userdata::emplace<Spline>(L, std::move(points), kind, closed);
    return 1;
}

int SplineLua::point(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Spline>(L, 1).getPoint(lua::Stack::read<float>(L, 2)));
    return 1;
}

int SplineLua::tangent(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Spline>(L, 1).getTangent(lua::Stack::read<float>(L, 2)));
    return 1;
}

int SplineLua::pointAtDistance(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Spline>(L, 1).getPointAtDistance(lua::Stack::read<float>(L, 2)));
    return 1;
}

int SplineLua::tangentAtDistance(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Spline>(L, 1).getTangentAtDistance(lua::Stack::read<float>(L, 2)));
    return 1;
}

int SplineLua::parameterAtDistance(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Spline>(L, 1).getParameterAtDistance(lua::Stack::read<float>(L, 2)));
    return 1;
}

int SplineLua::sample(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Spline>(L, 1).sample(lua::Stack::read<std::size_t>(L, 2)));
    return 1;
}

int SplineLua::sampleByDistance(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Spline>(L, 1).sampleByDistance(lua::Stack::read<float>(L, 2)));
    return 1;
}

int SplineLua::getLength(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Spline>(L, 1).getLength());
    return 1;
}

int SplineLua::getKind(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Spline>(L, 1).getKind());
    return 1;
}

int SplineLua::isClosed(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Spline>(L, 1).isClosed());
    return 1;
}

int SplineLua::getPoints(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Spline>(L, 1).getPoints());
    return 1;
}

int SplineLua::getSegmentCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Spline>(L, 1).getSegmentCount());
    return 1;
}

void SplineLua::install(lua_State* L) {
    lua::ClassBuilder<Spline>(L).function("point", &lua::Binding::native<&point>).function("tangent", &lua::Binding::native<&tangent>).function("pointAtDistance", &lua::Binding::native<&pointAtDistance>).function("tangentAtDistance", &lua::Binding::native<&tangentAtDistance>).function("parameterAtDistance", &lua::Binding::native<&parameterAtDistance>).function("sample", &lua::Binding::native<&sample>).function("sampleByDistance", &lua::Binding::native<&sampleByDistance>).property("length", &getLength).property("kind", &getKind).property("closed", &isClosed).property("points", &getPoints).property("segmentCount", &getSegmentCount).install();
}

void SplineLua::addFunctions(lua_State* L) {
    lua_pushcfunction(L, &lua::Binding::native<&newSpline>);
    lua_setfield(L, -2, "spline");
}

} // namespace haylen::math
