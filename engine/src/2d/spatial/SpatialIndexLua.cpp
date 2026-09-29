#include "2d/spatial/SpatialIndexLua.hpp"

#include <limits>
#include <utility>

#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/spatial/AabbTree.hpp"
#include "haylen/2d/spatial/HashGrid.hpp"
#include "haylen/2d/spatial/KdTree.hpp"
#include "haylen/2d/spatial/QuadTree.hpp"
#include "haylen/2d/spatial/ScreenPicker.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/math/Ray.hpp"
#include "math/RaycastLua.hpp"

namespace haylen::lua {

template <> struct Type<spatial2d::ScriptedIndex<spatial2d::HashGrid>> {
    static constexpr const char* name = "haylen.SpatialHash";
    using Storage = spatial2d::ScriptedIndex<spatial2d::HashGrid>;
};

template <> struct Type<spatial2d::ScriptedIndex<spatial2d::QuadTree>> {
    static constexpr const char* name = "haylen.QuadTree";
    using Storage = spatial2d::ScriptedIndex<spatial2d::QuadTree>;
};

template <> struct Type<spatial2d::ScriptedIndex<spatial2d::AabbTree>> {
    static constexpr const char* name = "haylen.AabbTree";
    using Storage = spatial2d::ScriptedIndex<spatial2d::AabbTree>;
};

template <> struct Type<spatial2d::ScriptedIndex<spatial2d::KdTree>> {
    static constexpr const char* name = "haylen.KdTree";
    using Storage = spatial2d::ScriptedIndex<spatial2d::KdTree>;
};

} // namespace haylen::lua

namespace haylen::spatial2d {

void SpatialIndexLua::resetTables(lua_State* L, int owner) {
    lua_newtable(L);
    lua::Userdata::setField(L, owner, "ids", -1);
    lua_newtable(L);
    lua::Userdata::setField(L, owner, "values", -1);
    lua_pop(L, 2);
}

std::optional<std::uint64_t> SpatialIndexLua::idOf(lua_State* L, int valueIndex) {
    const int value = lua_absindex(L, valueIndex);
    lua::Userdata::pushField(L, 1, "ids");
    lua_pushvalue(L, value);
    lua_rawget(L, -2);
    std::optional<std::uint64_t> id;
    if (lua_isinteger(L, -1) != 0) {
        id = static_cast<std::uint64_t>(lua_tointeger(L, -1));
    }
    lua_pop(L, 2);
    return id;
}

void SpatialIndexLua::remember(lua_State* L, std::uint64_t id) {
    lua::Userdata::pushField(L, 1, "ids");
    lua_pushvalue(L, 2);
    lua_pushinteger(L, static_cast<lua_Integer>(id));
    lua_rawset(L, -3);
    lua::Userdata::pushField(L, 1, "values");
    lua_pushvalue(L, 2);
    lua_rawseti(L, -2, static_cast<lua_Integer>(id));
    lua_pop(L, 2);
}

template <typename Structure, typename Store> void SpatialIndexLua::store(lua_State* L, ScriptedIndex<Structure>& self, Store&& storeEntry) {
    luaL_argcheck(L, !lua_isnoneornil(L, 2), 2, "a value to store is required");
    const std::optional<std::uint64_t> existing = idOf(L, 2);
    const std::uint64_t id = existing.value_or(self.nextId);
    storeEntry(id);
    if (!existing) {
        remember(L, id);
        ++self.nextId;
    }
}

void SpatialIndexLua::pushValues(lua_State* L, const std::vector<std::uint64_t>& ids) {
    lua::Userdata::pushField(L, 1, "values");
    const int values = lua_gettop(L);
    lua_createtable(L, static_cast<int>(ids.size()), 0);
    for (std::size_t index = 0; index < ids.size(); ++index) {
        lua_rawgeti(L, values, static_cast<lua_Integer>(ids[index]));
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    lua_remove(L, values);
}

void SpatialIndexLua::pushHits(lua_State* L, const std::vector<RayHit>& hits, float length) {
    lua::Userdata::pushField(L, 1, "values");
    const int values = lua_gettop(L);
    lua_createtable(L, static_cast<int>(hits.size()), 0);
    for (std::size_t index = 0; index < hits.size(); ++index) {
        const RayHit& hit = hits[index];
        math::RaycastLua::pushHit(L, {.point = hit.point, .normal = hit.normal, .distance = hit.distance}, length);
        lua_rawgeti(L, values, static_cast<lua_Integer>(hit.id));
        lua_setfield(L, -2, "value");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    lua_remove(L, values);
}

int SpatialIndexLua::newHash(lua_State* L) {
    HashGrid grid(lua::Stack::read<float>(L, 1));
    lua::Userdata::emplace<ScriptedIndex<HashGrid>>(L, ScriptedIndex<HashGrid>{.index = std::move(grid)});
    resetTables(L, lua_gettop(L));
    return 1;
}

// Creates a quadtree with newQuadTree(rect[, {maxEntries = 8, maxDepth = 8}]).
int SpatialIndexLua::newQuadTree(lua_State* L) {
    QuadTree::Settings settings;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kQuadTreeFields});
        lua::Table::readField(L, 2, "maxEntries", settings.maxEntries);
        lua::Table::readField(L, 2, "maxDepth", settings.maxDepth);
    }
    QuadTree tree(lua::Stack::read<math::Rect>(L, 1), settings);
    lua::Userdata::emplace<ScriptedIndex<QuadTree>>(L, ScriptedIndex<QuadTree>{.index = std::move(tree)});
    resetTables(L, lua_gettop(L));
    return 1;
}

int SpatialIndexLua::newAabbTree(lua_State* L) {
    AabbTree tree(lua_isnoneornil(L, 1) ? 4.0F : lua::Stack::read<float>(L, 1));
    lua::Userdata::emplace<ScriptedIndex<AabbTree>>(L, ScriptedIndex<AabbTree>{.index = std::move(tree)});
    resetTables(L, lua_gettop(L));
    return 1;
}

int SpatialIndexLua::newKdTree(lua_State* L) {
    lua::Userdata::emplace<ScriptedIndex<KdTree>>(L, ScriptedIndex<KdTree>{});
    resetTables(L, lua_gettop(L));
    return 1;
}

// Stores a value with set(value, rect), or moves it when it is already stored.
template <typename Structure> int SpatialIndexLua::set(lua_State* L) {
    ScriptedIndex<Structure>& self = lua::Userdata::check<ScriptedIndex<Structure>>(L, 1);
    const math::Rect rect = lua::Stack::read<math::Rect>(L, 3);
    store(L, self, [&self, &rect](std::uint64_t id) { self.index.set(id, rect); });
    return 0;
}

// Stores a value at a point with set(value, x, y[, radius]), or moves it when it is already stored.
int SpatialIndexLua::setPoint(lua_State* L) {
    ScriptedIndex<KdTree>& self = lua::Userdata::check<ScriptedIndex<KdTree>>(L, 1);
    const math::Vec2 point{lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)};
    const float radius = lua_isnoneornil(L, 5) ? 0.0F : lua::Stack::read<float>(L, 5);
    store(L, self, [&self, point, radius](std::uint64_t id) { self.index.set(id, point, radius); });
    return 0;
}

template <typename Structure> int SpatialIndexLua::remove(lua_State* L) {
    ScriptedIndex<Structure>& self = lua::Userdata::check<ScriptedIndex<Structure>>(L, 1);
    const std::optional<std::uint64_t> id = idOf(L, 2);
    if (!id) {
        lua::Stack::push(L, false);
        return 1;
    }

    self.index.remove(*id);
    lua::Userdata::pushField(L, 1, "ids");
    lua_pushvalue(L, 2);
    lua_pushnil(L);
    lua_rawset(L, -3);
    lua::Userdata::pushField(L, 1, "values");
    lua_pushnil(L);
    lua_rawseti(L, -2, static_cast<lua_Integer>(*id));
    lua_pop(L, 2);
    lua::Stack::push(L, true);
    return 1;
}

template <typename Structure> int SpatialIndexLua::has(lua_State* L) {
    (void)lua::Userdata::check<ScriptedIndex<Structure>>(L, 1);
    lua::Stack::push(L, idOf(L, 2).has_value());
    return 1;
}

template <typename Structure> int SpatialIndexLua::bounds(lua_State* L) {
    ScriptedIndex<Structure>& self = lua::Userdata::check<ScriptedIndex<Structure>>(L, 1);
    const std::optional<std::uint64_t> id = idOf(L, 2);
    lua::Stack::push(L, id ? self.index.getBounds(*id) : std::nullopt);
    return 1;
}

int SpatialIndexLua::position(lua_State* L) {
    ScriptedIndex<KdTree>& self = lua::Userdata::check<ScriptedIndex<KdTree>>(L, 1);
    const std::optional<std::uint64_t> id = idOf(L, 2);
    lua::Stack::push(L, id ? self.index.getPoint(*id) : std::nullopt);
    return 1;
}

int SpatialIndexLua::build(lua_State* L) {
    lua::Userdata::check<ScriptedIndex<KdTree>>(L, 1).index.build();
    return 0;
}

int SpatialIndexLua::isBuilt(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedIndex<KdTree>>(L, 1).index.isBuilt());
    return 1;
}

template <typename Structure> int SpatialIndexLua::query(lua_State* L) {
    std::vector<std::uint64_t> ids;
    lua::Userdata::check<ScriptedIndex<Structure>>(L, 1).index.query(lua::Stack::read<math::Rect>(L, 2), ids);
    pushValues(L, ids);
    return 1;
}

template <typename Structure> int SpatialIndexLua::queryCircle(lua_State* L) {
    std::vector<std::uint64_t> ids;
    lua::Userdata::check<ScriptedIndex<Structure>>(L, 1).index.queryCircle({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}, lua::Stack::read<float>(L, 4), ids);
    pushValues(L, ids);
    return 1;
}

template <typename Structure> int SpatialIndexLua::queryPoint(lua_State* L) {
    std::vector<std::uint64_t> ids;
    lua::Userdata::check<ScriptedIndex<Structure>>(L, 1).index.queryPoint({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}, ids);
    pushValues(L, ids);
    return 1;
}

// Casts a ray with raycast(x1, y1, x2, y2[, limit]) and returns the hits from the start on, each with the value it hit.
template <typename Structure> int SpatialIndexLua::raycast(lua_State* L) {
    const Structure& structure = lua::Userdata::check<ScriptedIndex<Structure>>(L, 1).index;
    const math::Ray ray = math::Ray::between({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}, {lua::Stack::read<float>(L, 4), lua::Stack::read<float>(L, 5)});
    const auto limit = lua_isnoneornil(L, 6) ? std::size_t{0} : lua::Stack::read<std::size_t>(L, 6);
    std::vector<RayHit> hits;
    structure.raycast(ray, limit, hits);
    pushHits(L, hits, ray.length);
    return 1;
}

// Finds the closest value with nearest(x, y, maxDistance[, accept]), where accept(value) can reject candidates.
template <typename Structure> int SpatialIndexLua::nearest(lua_State* L) {
    const Structure& structure = lua::Userdata::check<ScriptedIndex<Structure>>(L, 1).index;
    const math::Vec2 point{lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)};
    const auto maxDistance = lua::Stack::read<float>(L, 4);
    const bool filtered = !lua_isnoneornil(L, 5);
    if (filtered) {
        luaL_checktype(L, 5, LUA_TFUNCTION);
    }

    // Accept runs on every candidate from the closest one on, and it may remove values or clear the structure, which replaces the values table, so each candidate is looked up in the current table and a value that is gone is skipped.
    std::vector<Neighbor> candidates;
    structure.nearest(point, filtered ? structure.size() : 1, maxDistance, candidates);
    for (const Neighbor& candidate : candidates) {
        lua::Userdata::pushField(L, 1, "values");
        const int type = lua_rawgeti(L, -1, static_cast<lua_Integer>(candidate.id));
        lua_remove(L, -2);
        if (type == LUA_TNIL) {
            lua_pop(L, 1);
            continue;
        }
        if (!filtered) {
            return 1;
        }
        lua_pushvalue(L, 5);
        lua_pushvalue(L, -2);
        lua_call(L, 1, 1);
        const bool accepted = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
        if (accepted) {
            return 1;
        }
        lua_pop(L, 1);
    }
    lua_pushnil(L);
    return 1;
}

// Lists the closest values with kNearest(x, y, count[, maxDistance]), closest first.
template <typename Structure> int SpatialIndexLua::nearestList(lua_State* L) {
    const Structure& structure = lua::Userdata::check<ScriptedIndex<Structure>>(L, 1).index;
    const math::Vec2 point{lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)};
    const auto count = lua::Stack::read<std::size_t>(L, 4);
    const float maxDistance = lua_isnoneornil(L, 5) ? std::numeric_limits<float>::infinity() : lua::Stack::read<float>(L, 5);
    std::vector<Neighbor> neighbors;
    structure.nearest(point, count, maxDistance, neighbors);

    std::vector<std::uint64_t> ids;
    ids.reserve(neighbors.size());
    for (const Neighbor& neighbor : neighbors) {
        ids.push_back(neighbor.id);
    }
    pushValues(L, ids);
    return 1;
}

// Lists the values under a screen point with pick(camera, x, y), where the point is in design coordinates like pointer positions.
template <typename Structure> int SpatialIndexLua::pick(lua_State* L) {
    const Structure& structure = lua::Userdata::check<ScriptedIndex<Structure>>(L, 1).index;
    const ScreenPicker picker(lua::Userdata::check<graphics2d::Camera>(L, 2), lua::Runtime::getEngine(L).getViewport().getVisibleRect());
    std::vector<std::uint64_t> ids;
    picker.pick(structure, {lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)}, ids);
    pushValues(L, ids);
    return 1;
}

template <typename Structure> int SpatialIndexLua::clear(lua_State* L) {
    lua::Userdata::check<ScriptedIndex<Structure>>(L, 1).index.clear();
    resetTables(L, 1);
    return 0;
}

template <typename Structure> int SpatialIndexLua::size(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedIndex<Structure>>(L, 1).index.size());
    return 1;
}

int SpatialIndexLua::cellSize(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedIndex<HashGrid>>(L, 1).index.getCellSize());
    return 1;
}

int SpatialIndexLua::area(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedIndex<QuadTree>>(L, 1).index.getArea());
    return 1;
}

int SpatialIndexLua::nodeCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedIndex<QuadTree>>(L, 1).index.getNodeCount());
    return 1;
}

int SpatialIndexLua::height(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedIndex<AabbTree>>(L, 1).index.getHeight());
    return 1;
}

int SpatialIndexLua::margin(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedIndex<AabbTree>>(L, 1).index.getMargin());
    return 1;
}

template <typename Structure> lua::ClassBuilder<ScriptedIndex<Structure>>& SpatialIndexLua::addQueries(lua::ClassBuilder<ScriptedIndex<Structure>>& builder) {
    return builder.function("remove", &lua::Binding::native<&remove<Structure>>).function("has", &lua::Binding::native<&has<Structure>>).function("query", &lua::Binding::native<&query<Structure>>).function("queryCircle", &lua::Binding::native<&queryCircle<Structure>>).function("queryPoint", &lua::Binding::native<&queryPoint<Structure>>).function("raycast", &lua::Binding::native<&raycast<Structure>>).function("nearest", &lua::Binding::native<&nearest<Structure>>).function("kNearest", &lua::Binding::native<&nearestList<Structure>>).function("pick", &lua::Binding::native<&pick<Structure>>).function("clear", &lua::Binding::native<&clear<Structure>>).property("size", &size<Structure>);
}

void SpatialIndexLua::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newHash", &lua::Binding::native<&newHash>}, {"newQuadTree", &lua::Binding::native<&newQuadTree>}, {"newAabbTree", &lua::Binding::native<&newAabbTree>}, {"newKdTree", &lua::Binding::native<&newKdTree>}, {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

void SpatialIndexLua::install(lua_State* L) {
    lua::ClassBuilder<ScriptedIndex<HashGrid>> hash(L);
    addQueries(hash).function("set", &lua::Binding::native<&set<HashGrid>>).function("bounds", &lua::Binding::native<&bounds<HashGrid>>).property("cellSize", &cellSize).install();

    lua::ClassBuilder<ScriptedIndex<QuadTree>> quadTree(L);
    addQueries(quadTree).function("set", &lua::Binding::native<&set<QuadTree>>).function("bounds", &lua::Binding::native<&bounds<QuadTree>>).property("area", &area).property("nodeCount", &nodeCount).install();

    lua::ClassBuilder<ScriptedIndex<AabbTree>> aabbTree(L);
    addQueries(aabbTree).function("set", &lua::Binding::native<&set<AabbTree>>).function("bounds", &lua::Binding::native<&bounds<AabbTree>>).property("height", &height).property("margin", &margin).install();

    lua::ClassBuilder<ScriptedIndex<KdTree>> kdTree(L);
    addQueries(kdTree).function("set", &lua::Binding::native<&setPoint>).function("position", &lua::Binding::native<&position>).function("build", &lua::Binding::native<&build>).property("built", &isBuilt).install();
}

} // namespace haylen::spatial2d
