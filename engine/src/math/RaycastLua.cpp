#include "math/RaycastLua.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/math/Ray.hpp"
#include "haylen/math/Raycast.hpp"
#include "haylen/math/Segment.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::math {

void RaycastLua::pushHit(lua_State* L, const RayHit& hit, float length) {
    lua_createtable(L, 0, 7);
    lua::Stack::push(L, hit.point.x);
    lua_setfield(L, -2, "x");
    lua::Stack::push(L, hit.point.y);
    lua_setfield(L, -2, "y");
    lua::Stack::push(L, hit.normal.x);
    lua_setfield(L, -2, "normalX");
    lua::Stack::push(L, hit.normal.y);
    lua_setfield(L, -2, "normalY");
    lua::Stack::push(L, hit.distance);
    lua_setfield(L, -2, "distance");
    lua::Stack::push(L, std::isfinite(length) && length > 0.0F ? hit.distance / length : 0.0F);
    lua_setfield(L, -2, "fraction");
}

void RaycastLua::setIndex(lua_State* L, const RayHit& hit) {
    lua_pushinteger(L, static_cast<lua_Integer>(hit.index + 1));
    lua_setfield(L, -2, "index");
}

void RaycastLua::pushResult(lua_State* L, const std::optional<RayHit>& hit, float length, bool indexed) {
    if (!hit) {
        lua_pushnil(L);
        return;
    }
    pushHit(L, *hit, length);
    if (indexed) {
        setIndex(L, *hit);
    }
}

int RaycastLua::raycastSegment(lua_State* L) {
    const Ray ray = Ray::between(lua::Stack::read<Vec2>(L, 1), lua::Stack::read<Vec2>(L, 2));
    pushResult(L, Raycast::segment(ray, lua::Stack::read<Segment>(L, 3)), ray.length, false);
    return 1;
}

int RaycastLua::raycastRect(lua_State* L) {
    const Ray ray = Ray::between(lua::Stack::read<Vec2>(L, 1), lua::Stack::read<Vec2>(L, 2));
    pushResult(L, Raycast::rect(ray, lua::Stack::read<Rect>(L, 3)), ray.length, false);
    return 1;
}

int RaycastLua::raycastCircle(lua_State* L) {
    const Ray ray = Ray::between(lua::Stack::read<Vec2>(L, 1), lua::Stack::read<Vec2>(L, 2));
    pushResult(L, Raycast::circle(ray, lua::Stack::read<Circle>(L, 3)), ray.length, false);
    return 1;
}

int RaycastLua::raycastPolygon(lua_State* L) {
    const Ray ray = Ray::between(lua::Stack::read<Vec2>(L, 1), lua::Stack::read<Vec2>(L, 2));
    pushResult(L, Raycast::polygon(ray, lua::Stack::read<std::vector<Vec2>>(L, 3)), ray.length, true);
    return 1;
}

// Casts against a polyline with `raycastChain(from, to, points[, loop])`, where a loop also joins the last point to the first.
int RaycastLua::raycastChain(lua_State* L) {
    const Ray ray = Ray::between(lua::Stack::read<Vec2>(L, 1), lua::Stack::read<Vec2>(L, 2));
    const bool loop = !lua_isnoneornil(L, 4) && lua::Stack::read<bool>(L, 4);
    pushResult(L, Raycast::chain(ray, lua::Stack::read<std::vector<Vec2>>(L, 3), loop), ray.length, true);
    return 1;
}

int RaycastLua::raycastSegments(lua_State* L) {
    const Ray ray = Ray::between(lua::Stack::read<Vec2>(L, 1), lua::Stack::read<Vec2>(L, 2));
    pushResult(L, Raycast::segments(ray, lua::Stack::read<std::vector<Segment>>(L, 3)), ray.length, true);
    return 1;
}

// Returns every crossed segment with `raycastSegmentsAll(from, to, segments[, limit])`, sorted by distance and cut to the limit.
int RaycastLua::raycastSegmentsAll(lua_State* L) {
    const Ray ray = Ray::between(lua::Stack::read<Vec2>(L, 1), lua::Stack::read<Vec2>(L, 2));
    const auto limit = lua_isnoneornil(L, 4) ? std::size_t{0} : lua::Stack::read<std::size_t>(L, 4);
    std::vector<RayHit> hits;
    Raycast::segmentsAll(ray, lua::Stack::read<std::vector<Segment>>(L, 3), limit, hits);

    lua_createtable(L, static_cast<int>(hits.size()), 0);
    for (std::size_t index = 0; index < hits.size(); ++index) {
        pushHit(L, hits[index], ray.length);
        setIndex(L, hits[index]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    return 1;
}

// Bounces a ray off segments with `bounceRay(origin, direction, length, bounces, segments)` and returns the bounce hits and the point where the path ends. Hit distances and fractions measure the whole path up to each bounce.
int RaycastLua::bounceRay(lua_State* L) {
    const Vec2 origin = lua::Stack::read<Vec2>(L, 1);
    const Vec2 direction = lua::Stack::read<Vec2>(L, 2).getNormalized();
    luaL_argcheck(L, !direction.isZero(), 2, "the direction must not be zero");
    const auto length = lua::Stack::read<float>(L, 3);
    const auto bounces = lua::Stack::read<int>(L, 4);
    luaL_argcheck(L, bounces >= 0, 4, "the bounce count must not be negative");
    const std::vector<Segment> walls = lua::Stack::read<std::vector<Segment>>(L, 5);

    std::vector<RayHit> hits;
    const Ray last = Raycast::bounce(Ray{origin, direction, length}, bounces, [&walls](const Ray& ray) { return Raycast::segments(ray, walls); }, hits);

    lua_createtable(L, static_cast<int>(hits.size()), 0);
    float traveled = 0.0F;
    for (std::size_t index = 0; index < hits.size(); ++index) {
        traveled += hits[index].distance;
        RayHit along = hits[index];
        along.distance = traveled;
        pushHit(L, along, length);
        setIndex(L, along);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    const Vec2 end = last.getEnd();
    lua::Stack::push(L, end.x);
    lua::Stack::push(L, end.y);
    return 3;
}

// Casts a fan of rays against segments with `rayFan(origin, angle, spread, count, length, segments)`. Each entry is the hit of one ray, or `false` when that ray hit nothing.
int RaycastLua::rayFan(lua_State* L) {
    const Vec2 origin = lua::Stack::read<Vec2>(L, 1);
    const auto angle = lua::Stack::read<float>(L, 2);
    const auto spread = lua::Stack::read<float>(L, 3);
    const auto count = lua::Stack::read<std::size_t>(L, 4);
    const auto length = lua::Stack::read<float>(L, 5);
    const std::vector<Segment> walls = lua::Stack::read<std::vector<Segment>>(L, 6);

    std::vector<std::optional<RayHit>> results;
    Raycast::fan(origin, angle, spread, count, length, [&walls](const Ray& ray) { return Raycast::segments(ray, walls); }, results);

    lua_createtable(L, static_cast<int>(results.size()), 0);
    for (std::size_t index = 0; index < results.size(); ++index) {
        if (results[index]) {
            pushHit(L, *results[index], length);
            setIndex(L, *results[index]);
        } else {
            lua_pushboolean(L, 0);
        }
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    return 1;
}

void RaycastLua::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"raycastSegment", &lua::Binding::native<&raycastSegment>}, {"raycastRect", &lua::Binding::native<&raycastRect>}, {"raycastCircle", &lua::Binding::native<&raycastCircle>}, {"raycastPolygon", &lua::Binding::native<&raycastPolygon>}, {"raycastChain", &lua::Binding::native<&raycastChain>}, {"raycastSegments", &lua::Binding::native<&raycastSegments>}, {"raycastSegmentsAll", &lua::Binding::native<&raycastSegmentsAll>}, {"bounceRay", &lua::Binding::native<&bounceRay>}, {"rayFan", &lua::Binding::native<&rayFan>}, {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

} // namespace haylen::math
