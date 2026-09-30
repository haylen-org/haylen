#include "2d/physics/WorldRaycastLua.hpp"

#include <cmath>
#include <vector>

#include "2d/physics/Physics2DLua.hpp"
#include "2d/physics/ShapeLua.hpp"
#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/physics/RayBatch.hpp"
#include "haylen/2d/physics/RayDebugDraw.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/2d/spatial/ScreenPicker.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/math/Ray.hpp"
#include "haylen/math/Raycast.hpp"

namespace haylen::lua {

template <> struct Type<physics2d::RayBatch> {
    static constexpr const char* name = "haylen.RayBatch";
    using Storage = physics2d::RayBatch;
};

template <> struct Type<physics2d::RayDebugDraw> {
    static constexpr const char* name = "haylen.RayDebugDraw";
    using Storage = physics2d::RayDebugDraw;
};

} // namespace haylen::lua

namespace haylen::physics2d {

Raycaster::Filter WorldRaycastLua::readFilter(lua_State* L, int index, bool withLimit) {
    Raycaster::Filter filter;
    if (lua_isnoneornil(L, index)) {
        return filter;
    }
    luaL_checktype(L, index, LUA_TTABLE);
    if (withLimit) {
        lua::Table::checkFields(L, index, {kFilterFields, kLimitFields});
    } else {
        lua::Table::checkFields(L, index, {kFilterFields});
    }
    filter.collision = ShapeLua::readFilter(L, index, {});

    // The `accept` function stays on the stack while the cast runs, and it receives the hit tables of the world at index 1.
    if (lua_getfield(L, index, "accept") == LUA_TNIL) {
        lua_pop(L, 1);
        return filter;
    }
    luaL_checktype(L, -1, LUA_TFUNCTION);
    const int function = lua_gettop(L);
    // clang-format off
    filter.accept = [L, function](const RaycastHit& hit) {
        lua_pushvalue(L, function);
        pushHit(L, 1, hit);
        lua_call(L, 1, 1);
        const bool accepted = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
        return accepted;
    };
    // clang-format on
    return filter;
}

std::size_t WorldRaycastLua::readLimit(lua_State* L, int index) {
    std::size_t limit = 0;
    if (lua_istable(L, index)) {
        lua::Table::readField(L, index, "limit", limit);
    }
    return limit;
}

void WorldRaycastLua::pushHit(lua_State* L, int worldIndex, const RaycastHit& hit) {
    const int world = lua_absindex(L, worldIndex);
    lua_createtable(L, 0, 8);
    Physics2DLua::push(L, world, hit.shape);
    lua_setfield(L, -2, "shape");
    if (hit.shape.isValid()) {
        Physics2DLua::push(L, world, hit.shape.getBody());
    } else {
        lua_pushnil(L);
    }
    lua_setfield(L, -2, "body");
    lua::Stack::push(L, hit.point.x);
    lua_setfield(L, -2, "x");
    lua::Stack::push(L, hit.point.y);
    lua_setfield(L, -2, "y");
    lua::Stack::push(L, hit.normal.x);
    lua_setfield(L, -2, "normalX");
    lua::Stack::push(L, hit.normal.y);
    lua_setfield(L, -2, "normalY");
    lua::Stack::push(L, hit.fraction);
    lua_setfield(L, -2, "fraction");
    lua::Stack::push(L, hit.distance);
    lua_setfield(L, -2, "distance");
}

void WorldRaycastLua::pushResult(lua_State* L, const std::optional<RaycastHit>& hit) {
    if (!hit) {
        lua_pushnil(L);
        return;
    }
    pushHit(L, 1, *hit);
}

void WorldRaycastLua::record(lua_State* L, math::Vec2 from, math::Vec2 to, const std::optional<RaycastHit>& hit) {
    lua_getiuservalue(L, 1, 1);
    if (lua_getfield(L, -1, "rayDebug") == LUA_TUSERDATA) {
        const std::optional<RayDebugDraw::Hit> mark = hit ? std::optional<RayDebugDraw::Hit>{{.point = hit->point, .normal = hit->normal}} : std::nullopt;
        lua::Userdata::check<RayDebugDraw>(L, -1).add(lua::Runtime::getEngine(L).getClock().getFrameIndex(), from, to, mark);
    }
    lua_pop(L, 2);
}

// Returns the closest hit with `raycast(x1, y1, x2, y2[, filter])`, where the filter takes `category`, `mask`, `group` and `accept`.
int WorldRaycastLua::raycast(lua_State* L) {
    const World& world = lua::Userdata::check<World>(L, 1);
    const math::Vec2 from{lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)};
    const math::Vec2 to{lua::Stack::read<float>(L, 4), lua::Stack::read<float>(L, 5)};
    const std::optional<RaycastHit> hit = Raycaster(world).castRay(from, to, readFilter(L, 6));
    record(L, from, to, hit);
    pushResult(L, hit);
    return 1;
}

// Lists the hits in order with `raycastAll(x1, y1, x2, y2[, filter])`, where the filter also takes a `limit` of hits.
int WorldRaycastLua::raycastAll(lua_State* L) {
    const World& world = lua::Userdata::check<World>(L, 1);
    const math::Vec2 from{lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)};
    const math::Vec2 to{lua::Stack::read<float>(L, 4), lua::Stack::read<float>(L, 5)};
    std::vector<RaycastHit> hits;
    Raycaster(world).castRayAll(from, to, readFilter(L, 6, true), readLimit(L, 6), hits);
    record(L, from, to, hits.empty() ? std::nullopt : std::optional<RaycastHit>{hits.back()});

    lua_createtable(L, static_cast<int>(hits.size()), 0);
    for (std::size_t index = 0; index < hits.size(); ++index) {
        pushHit(L, 1, hits[index]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    return 1;
}

// Sweeps a circle with `castCircle(x, y, radius, dx, dy[, filter])`.
int WorldRaycastLua::castCircle(lua_State* L) {
    const World& world = lua::Userdata::check<World>(L, 1);
    const math::Vec2 center{lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)};
    const math::Vec2 translation{lua::Stack::read<float>(L, 5), lua::Stack::read<float>(L, 6)};
    const std::optional<RaycastHit> hit = Raycaster(world).castCircle(center, lua::Stack::read<float>(L, 4), translation, readFilter(L, 7));
    record(L, center, center + translation, hit);
    pushResult(L, hit);
    return 1;
}

// Sweeps a box with `castBox(x, y, width, height, rotation, dx, dy[, filter])`.
int WorldRaycastLua::castBox(lua_State* L) {
    const World& world = lua::Userdata::check<World>(L, 1);
    const math::Vec2 center{lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)};
    const math::Vec2 size{lua::Stack::read<float>(L, 4), lua::Stack::read<float>(L, 5)};
    const math::Vec2 translation{lua::Stack::read<float>(L, 7), lua::Stack::read<float>(L, 8)};
    const std::optional<RaycastHit> hit = Raycaster(world).castBox(center, size, lua::Stack::read<float>(L, 6), translation, readFilter(L, 9));
    record(L, center, center + translation, hit);
    pushResult(L, hit);
    return 1;
}

// Sweeps a capsule with `castCapsule(x1, y1, x2, y2, radius, dx, dy[, filter])`.
int WorldRaycastLua::castCapsule(lua_State* L) {
    const World& world = lua::Userdata::check<World>(L, 1);
    const math::Vec2 first{lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)};
    const math::Vec2 second{lua::Stack::read<float>(L, 4), lua::Stack::read<float>(L, 5)};
    const math::Vec2 translation{lua::Stack::read<float>(L, 7), lua::Stack::read<float>(L, 8)};
    const std::optional<RaycastHit> hit = Raycaster(world).castCapsule(first, second, lua::Stack::read<float>(L, 6), translation, readFilter(L, 9));
    const math::Vec2 middle = (first + second) * 0.5F;
    record(L, middle, middle + translation, hit);
    pushResult(L, hit);
    return 1;
}

// Sweeps a convex polygon with `castPolygon(points, dx, dy[, filter])`.
int WorldRaycastLua::castPolygon(lua_State* L) {
    const World& world = lua::Userdata::check<World>(L, 1);
    const std::vector<math::Vec2> points = lua::Stack::read<std::vector<math::Vec2>>(L, 2);
    const math::Vec2 translation{lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)};
    const std::optional<RaycastHit> hit = Raycaster(world).castPolygon(points, translation, readFilter(L, 5));
    math::Vec2 center;
    for (const math::Vec2 point : points) {
        center += point / static_cast<float>(points.size());
    }
    record(L, center, center + translation, hit);
    pushResult(L, hit);
    return 1;
}

// Bounces a ray with `bounceRay(x, y, dx, dy, length, bounces[, filter])` and returns the bounce hits and the point where the path ends. Hit distances and fractions measure the whole path up to each bounce.
int WorldRaycastLua::bounceRay(lua_State* L) {
    const World& world = lua::Userdata::check<World>(L, 1);
    const math::Vec2 origin{lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)};
    const math::Vec2 direction = math::Vec2{lua::Stack::read<float>(L, 4), lua::Stack::read<float>(L, 5)}.getNormalized();
    luaL_argcheck(L, !direction.isZero(), 4, "the direction must not be zero");
    const auto length = lua::Stack::read<float>(L, 6);
    const auto bounces = lua::Stack::read<int>(L, 7);
    luaL_argcheck(L, bounces >= 0, 7, "the bounce count must not be negative");

    std::vector<RaycastHit> hits;
    const math::Ray last = Raycaster(world).bounce({origin, direction, length}, bounces, readFilter(L, 8), hits);
    math::Vec2 legStart = origin;
    float traveled = 0.0F;
    lua_createtable(L, static_cast<int>(hits.size()), 0);
    for (std::size_t index = 0; index < hits.size(); ++index) {
        record(L, legStart, hits[index].point, hits[index]);
        legStart = hits[index].point;
        traveled += hits[index].distance;
        RaycastHit along = hits[index];
        along.distance = traveled;
        along.fraction = length > 0.0F ? traveled / length : 0.0F;
        pushHit(L, 1, along);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    const math::Vec2 end = last.getEnd();
    if (hits.empty() || end != hits.back().point) {
        record(L, last.origin, end, std::nullopt);
    }
    lua::Stack::push(L, end.x);
    lua::Stack::push(L, end.y);
    return 3;
}

// Casts a fan of rays with `rayFan(x, y, angle, spread, count, length[, filter])`. Each entry is the hit of one ray, or `false` when that ray hit nothing.
int WorldRaycastLua::rayFan(lua_State* L) {
    const Raycaster caster(lua::Userdata::check<World>(L, 1));
    const math::Vec2 origin{lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)};
    const auto angle = lua::Stack::read<float>(L, 4);
    const auto spread = lua::Stack::read<float>(L, 5);
    const auto count = lua::Stack::read<std::size_t>(L, 6);
    const auto length = lua::Stack::read<float>(L, 7);
    luaL_argcheck(L, std::isfinite(length), 7, "the length must be finite");
    const Raycaster::Filter filter = readFilter(L, 8);

    // The fan hands each ray to the cast, which records it with its hit.
    std::vector<std::optional<RaycastHit>> results;
    // clang-format off
    math::Raycast::fan(origin, angle, spread, count, length, [&](const math::Ray& ray) {
        std::optional<RaycastHit> hit = caster.castRay(ray.origin, ray.getEnd(), filter);
        record(L, ray.origin, ray.getEnd(), hit);
        return hit;
    }, results);
    // clang-format on

    lua_createtable(L, static_cast<int>(results.size()), 0);
    for (std::size_t index = 0; index < results.size(); ++index) {
        if (results[index]) {
            pushHit(L, 1, *results[index]);
        } else {
            lua_pushboolean(L, 0);
        }
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    return 1;
}

int WorldRaycastLua::lineOfSight(lua_State* L) {
    const World& world = lua::Userdata::check<World>(L, 1);
    const math::Vec2 from{lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)};
    const math::Vec2 to{lua::Stack::read<float>(L, 4), lua::Stack::read<float>(L, 5)};
    const std::optional<RaycastHit> blocker = Raycaster(world).castRay(from, to, readFilter(L, 6));
    record(L, from, to, blocker);
    lua::Stack::push(L, !blocker.has_value());
    return 1;
}

// Casts every ray of a batch with `raycastBatch(batch[, filter])`, spread over the job system. Batches take `category`, `mask` and `group`, but no `accept` function.
int WorldRaycastLua::raycastBatch(lua_State* L) {
    const World& world = lua::Userdata::check<World>(L, 1);
    RayBatch& batch = lua::Userdata::check<RayBatch>(L, 2);
    const Raycaster::Filter filter = readFilter(L, 3);
    if (filter.accept) {
        return luaL_error(L, "Ray batches take no \"accept\" function, because their rays run on worker threads.");
    }
    Raycaster(world).castBatch(batch, filter.collision, &lua::Runtime::getEngine(L).getJobs());

    // The batch keeps the world that cast it, which hands its shapes and bodies to Lua.
    lua_pushvalue(L, 1);
    lua_setiuservalue(L, 2, 1);
    for (std::size_t index = 0; index < batch.size(); ++index) {
        record(L, batch.getRay(index).start, batch.getRay(index).end, batch.getHit(index));
    }
    return 0;
}

// Lists the shapes under a screen point with `pick(camera, x, y[, filter])`, where the point is in design coordinates like pointer positions and the filter takes `category` and `mask`.
int WorldRaycastLua::pick(lua_State* L) {
    const World& world = lua::Userdata::check<World>(L, 1);
    const spatial2d::ScreenPicker picker(lua::Userdata::check<graphics2d::Camera>(L, 2), lua::Runtime::getEngine(L).getViewport().getVisibleRect());
    const math::Vec2 point = picker.toWorld({lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)});
    CollisionFilter filter;
    if (!lua_isnoneornil(L, 5)) {
        luaL_checktype(L, 5, LUA_TTABLE);
        lua::Table::checkFields(L, 5, {kPickFields});
        filter = ShapeLua::readFilter(L, 5, {});
    }
    Physics2DLua::pushList(L, 1, world.queryPoint(point, filter));
    return 1;
}

int WorldRaycastLua::isDebuggingRays(lua_State* L) {
    (void)lua::Userdata::check<World>(L, 1);
    lua_getiuservalue(L, 1, 1);
    lua::Stack::push(L, lua_getfield(L, -1, "rayDebug") == LUA_TUSERDATA);
    return 1;
}

// Turns ray recording on or off with `world.debugRays = enabled`, which keeps a recorder in the world state table while on.
int WorldRaycastLua::setDebuggingRays(lua_State* L) {
    (void)lua::Userdata::check<World>(L, 1);
    const bool enabled = lua::Stack::read<bool>(L, 3);
    lua_getiuservalue(L, 1, 1);
    const bool active = lua_getfield(L, -1, "rayDebug") == LUA_TUSERDATA;
    lua_pop(L, 1);
    if (enabled == active) {
        return 0;
    }
    if (enabled) {
        lua::Userdata::emplace<RayDebugDraw>(L);
    } else {
        lua_pushnil(L);
    }
    lua_setfield(L, -2, "rayDebug");
    return 0;
}

// Draws the rays recorded since the last call with `debugDrawRays([order])` and clears them.
int WorldRaycastLua::debugDrawRays(lua_State* L) {
    (void)lua::Userdata::check<World>(L, 1);
    const graphics2d::DrawOrder order = lua::TypeConverter::readDrawOrder(L, 2);
    lua_getiuservalue(L, 1, 1);
    if (lua_getfield(L, -1, "rayDebug") == LUA_TUSERDATA) {
        lua::Userdata::check<RayDebugDraw>(L, -1).draw(lua::Runtime::getEngine(L).getRenderer2D(), order);
    }
    return 0;
}

std::size_t WorldRaycastLua::readBatchIndex(lua_State* L, int index) {
    const auto position = lua::Stack::read<lua_Integer>(L, index);
    const RayBatch& batch = lua::Userdata::check<RayBatch>(L, 1);
    luaL_argcheck(L, position >= 1 && static_cast<std::size_t>(position) <= batch.size(), index, "the ray index is outside the batch");
    return static_cast<std::size_t>(position - 1);
}

int WorldRaycastLua::newRayBatch(lua_State* L) {
    const std::size_t count = lua_isnoneornil(L, 1) ? std::size_t{0} : lua::Stack::read<std::size_t>(L, 1);
    lua::Userdata::emplace<RayBatch>(L).resize(count);
    return 1;
}

int WorldRaycastLua::batchSize(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<RayBatch>(L, 1).size());
    return 1;
}

int WorldRaycastLua::setBatchSize(lua_State* L) {
    lua::Userdata::check<RayBatch>(L, 1).resize(lua::Stack::read<std::size_t>(L, 3));
    return 0;
}

// Sets ray `i` with `setRay(i, x1, y1, x2, y2)`, counting from 1.
int WorldRaycastLua::batchSetRay(lua_State* L) {
    RayBatch& batch = lua::Userdata::check<RayBatch>(L, 1);
    batch.setRay(readBatchIndex(L, 2), {lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)}, {lua::Stack::read<float>(L, 5), lua::Stack::read<float>(L, 6)});
    return 0;
}

int WorldRaycastLua::batchRay(lua_State* L) {
    const math::Segment& ray = lua::Userdata::check<RayBatch>(L, 1).getRay(readBatchIndex(L, 2));
    lua::Stack::push(L, ray.start.x);
    lua::Stack::push(L, ray.start.y);
    lua::Stack::push(L, ray.end.x);
    lua::Stack::push(L, ray.end.y);
    return 4;
}

// Returns `false` for a ray that hit nothing, and otherwise `true`, `x`, `y`, `normalX`, `normalY` and `fraction`, so reading results creates no tables.
int WorldRaycastLua::batchHit(lua_State* L) {
    const std::optional<RaycastHit>& hit = lua::Userdata::check<RayBatch>(L, 1).getHit(readBatchIndex(L, 2));
    if (!hit) {
        lua::Stack::push(L, false);
        return 1;
    }
    lua::Stack::push(L, true);
    lua::Stack::push(L, hit->point.x);
    lua::Stack::push(L, hit->point.y);
    lua::Stack::push(L, hit->normal.x);
    lua::Stack::push(L, hit->normal.y);
    lua::Stack::push(L, hit->fraction);
    return 6;
}

int WorldRaycastLua::batchShape(lua_State* L) {
    const std::optional<RaycastHit>& hit = lua::Userdata::check<RayBatch>(L, 1).getHit(readBatchIndex(L, 2));
    if (!hit || lua_getiuservalue(L, 1, 1) != LUA_TUSERDATA) {
        lua_pushnil(L);
        return 1;
    }
    Physics2DLua::push(L, -1, hit->shape);
    return 1;
}

int WorldRaycastLua::batchBody(lua_State* L) {
    const std::optional<RaycastHit>& hit = lua::Userdata::check<RayBatch>(L, 1).getHit(readBatchIndex(L, 2));
    if (!hit || !hit->shape.isValid() || lua_getiuservalue(L, 1, 1) != LUA_TUSERDATA) {
        lua_pushnil(L);
        return 1;
    }
    Physics2DLua::push(L, -1, hit->shape.getBody());
    return 1;
}

// Draws one ray with `drawRay(x1, y1, x2, y2[, hit[, order]])`, where `hit` is a hit table of any cast, or `nil` or `false` for a miss.
int WorldRaycastLua::drawRay(lua_State* L) {
    const math::Vec2 from{lua::Stack::read<float>(L, 1), lua::Stack::read<float>(L, 2)};
    const math::Vec2 to{lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)};
    std::optional<RayDebugDraw::Hit> hit;
    if (lua_istable(L, 5)) {
        RayDebugDraw::Hit mark;
        lua::Table::readField(L, 5, "x", mark.point.x);
        lua::Table::readField(L, 5, "y", mark.point.y);
        lua::Table::readField(L, 5, "normalX", mark.normal.x);
        lua::Table::readField(L, 5, "normalY", mark.normal.y);
        hit = mark;
    }
    RayDebugDraw::drawRay(lua::Runtime::getEngine(L).getRenderer2D(), from, to, hit, lua::TypeConverter::readDrawOrder(L, 6));
    return 0;
}

void WorldRaycastLua::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newRayBatch", &lua::Binding::native<&newRayBatch>},
        {"drawRay", &lua::Binding::native<&drawRay>},
        {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

void WorldRaycastLua::install(lua_State* L) {
    lua::ClassBuilder<RayBatch>(L).property("size", &batchSize, &lua::Binding::native<&setBatchSize>).function("setRay", &lua::Binding::native<&batchSetRay>).function("ray", &lua::Binding::native<&batchRay>).function("hit", &lua::Binding::native<&batchHit>).function("shape", &lua::Binding::native<&batchShape>).function("body", &lua::Binding::native<&batchBody>).install();
    lua::ClassBuilder<RayDebugDraw>(L).install();
}

} // namespace haylen::physics2d
