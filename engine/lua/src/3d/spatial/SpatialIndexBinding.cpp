#include "3d/spatial/SpatialIndexBinding.hpp"

#include <format>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>

#include "core/FloatBufferBinding.hpp"
#include "haylen/3d/spatial/AabbTree.hpp"
#include "haylen/3d/spatial/Bvh.hpp"
#include "haylen/3d/spatial/HashGrid.hpp"
#include "haylen/3d/spatial/KdTree.hpp"
#include "haylen/3d/spatial/Octree.hpp"
#include "haylen/core/FloatBuffer.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/math/RayHit3D.hpp"
#include "math/Geometry3DBinding.hpp"

namespace haylen::lua {

template <> struct Type<lua::spatial3d::ScriptedIndex<haylen::spatial3d::HashGrid>> {
    static constexpr const char* name = "haylen.HashGrid3D";
    using Storage = lua::spatial3d::ScriptedIndex<haylen::spatial3d::HashGrid>;
};

template <> struct Type<lua::spatial3d::ScriptedIndex<haylen::spatial3d::Octree>> {
    static constexpr const char* name = "haylen.Octree";
    using Storage = lua::spatial3d::ScriptedIndex<haylen::spatial3d::Octree>;
};

template <> struct Type<lua::spatial3d::ScriptedIndex<haylen::spatial3d::AabbTree>> {
    static constexpr const char* name = "haylen.AabbTree3D";
    using Storage = lua::spatial3d::ScriptedIndex<haylen::spatial3d::AabbTree>;
};

template <> struct Type<lua::spatial3d::ScriptedIndex<haylen::spatial3d::Bvh>> {
    static constexpr const char* name = "haylen.Bvh";
    using Storage = lua::spatial3d::ScriptedIndex<haylen::spatial3d::Bvh>;
};

template <> struct Type<lua::spatial3d::ScriptedIndex<haylen::spatial3d::KdTree>> {
    static constexpr const char* name = "haylen.KdTree3D";
    using Storage = lua::spatial3d::ScriptedIndex<haylen::spatial3d::KdTree>;
};

} // namespace haylen::lua

namespace haylen::lua::spatial3d {

void SpatialIndexBinding::resetTables(lua_State* L, int owner) {
    lua_newtable(L);
    lua::Userdata::setField(L, owner, "ids", -1);
    lua_newtable(L);
    lua::Userdata::setField(L, owner, "values", -1);
    lua_pop(L, 2);
}

std::optional<std::uint64_t> SpatialIndexBinding::idOf(lua_State* L, int valueIndex) {
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

void SpatialIndexBinding::remember(lua_State* L, int valueIndex, std::uint64_t id) {
    const int value = lua_absindex(L, valueIndex);
    lua::Userdata::pushField(L, 1, "ids");
    lua_pushvalue(L, value);
    lua_pushinteger(L, static_cast<lua_Integer>(id));
    lua_rawset(L, -3);
    lua::Userdata::pushField(L, 1, "values");
    lua_pushvalue(L, value);
    lua_rawseti(L, -2, static_cast<lua_Integer>(id));
    lua_pop(L, 2);
}

void SpatialIndexBinding::forgetFrom(lua_State* L, std::uint64_t first, std::uint64_t end) {
    lua::Userdata::pushField(L, 1, "ids");
    lua::Userdata::pushField(L, 1, "values");
    for (std::uint64_t id = first; id < end; ++id) {
        lua_rawgeti(L, -1, static_cast<lua_Integer>(id));
        lua_pushnil(L);
        lua_rawset(L, -4);
        lua_pushnil(L);
        lua_rawseti(L, -2, static_cast<lua_Integer>(id));
    }

    lua_pop(L, 2);
}

template <typename Structure, typename Store> void SpatialIndexBinding::store(lua_State* L, ScriptedIndex<Structure>& self, Store&& storeEntry) {
    luaL_argcheck(L, !lua_isnoneornil(L, 2), 2, "a value to store is required");
    const std::optional<std::uint64_t> existing = idOf(L, 2);
    const std::uint64_t id = existing.value_or(self.nextId);
    storeEntry(id);

    if (!existing) {
        remember(L, 2, id);
        ++self.nextId;
    }
}

void SpatialIndexBinding::pushValues(lua_State* L, const std::vector<std::uint64_t>& ids, int results) {
    lua::Userdata::pushField(L, 1, "values");
    const int values = lua_gettop(L);
    if (results > 0) {
        luaL_checktype(L, results, LUA_TTABLE);
        lua_pushvalue(L, results);
    } else {
        lua_createtable(L, static_cast<int>(ids.size()), 0);
    }

    for (std::size_t index = 0; index < ids.size(); ++index) {
        lua_rawgeti(L, values, static_cast<lua_Integer>(ids[index]));
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    // A reused table keeps no items of an earlier, longer answer.
    for (auto index = static_cast<lua_Integer>(ids.size() + 1); results > 0; ++index) {
        const int type = lua_rawgeti(L, -1, index);
        lua_pop(L, 1);
        if (type == LUA_TNIL) {
            break;
        }

        lua_pushnil(L);
        lua_rawseti(L, -2, index);
    }

    lua_remove(L, values);
}

void SpatialIndexBinding::pushHits(lua_State* L, const std::vector<haylen::spatial3d::RayHit>& hits) {
    lua::Userdata::pushField(L, 1, "values");
    const int values = lua_gettop(L);
    lua_createtable(L, static_cast<int>(hits.size()), 0);
    for (std::size_t index = 0; index < hits.size(); ++index) {
        const haylen::spatial3d::RayHit& hit = hits[index];
        lua::math::Geometry3DBinding::pushHit(L, haylen::math::RayHit3D{.distance = hit.distance, .point = hit.point, .normal = hit.normal});
        lua_rawgeti(L, values, static_cast<lua_Integer>(hit.id));
        lua_setfield(L, -2, "value");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    lua_remove(L, values);
}

template <typename Structure, typename Apply> void SpatialIndexBinding::storeBatch(lua_State* L, ScriptedIndex<Structure>& self, std::size_t floats, Apply&& apply) {
    luaL_checktype(L, 2, LUA_TTABLE);
    const std::span<const float> numbers = lua::Userdata::check<haylen::core::FloatBuffer>(L, 3).getValues();
    const auto count = static_cast<std::size_t>(lua_rawlen(L, 2));
    if (numbers.size() < count * floats) {
        throw std::invalid_argument(std::format("The buffer of a batch of {} values needs {} numbers, but it holds {}.", count, count * floats, numbers.size()));
    }

    self.boxes.clear();
    self.spheres.clear();
    for (std::size_t index = 0; index < count; ++index) {
        const std::span<const float> entry = numbers.subspan(index * floats, floats);
        if (floats == kSphereFloats) {
            self.spheres.push_back({{entry[0], entry[1], entry[2]}, entry[3]});
        } else {
            self.boxes.push_back({{entry[0], entry[1], entry[2]}, {entry[3], entry[4], entry[5]}});
        }
    }

    // Values new to the structure take ids from the next one on, and a batch that the structure refuses forgets them again, so a refused batch changes nothing.
    const std::uint64_t firstNew = self.nextId;
    self.ids.clear();
    for (std::size_t index = 0; index < count; ++index) {
        const int type = lua_rawgeti(L, 2, static_cast<lua_Integer>(index + 1));
        if (type == LUA_TNIL) {
            forgetFrom(L, firstNew, self.nextId);
            self.nextId = firstNew;
            throw std::invalid_argument(std::format("Item {} of the values of a batch is nil, but every item needs a value to store.", index + 1));
        }

        const std::optional<std::uint64_t> existing = idOf(L, -1);
        if (!existing) {
            remember(L, -1, self.nextId++);
        }

        self.ids.push_back(existing.value_or(self.nextId - 1));
        lua_pop(L, 1);
    }

    try {
        apply();
    } catch (...) {
        forgetFrom(L, firstNew, self.nextId);
        self.nextId = firstNew;
        throw;
    }
}

int SpatialIndexBinding::newHashGrid(lua_State* L) {
    haylen::spatial3d::HashGrid grid(lua::Stack::read<float>(L, 1));
    lua::Userdata::emplace<ScriptedIndex<haylen::spatial3d::HashGrid>>(L, ScriptedIndex<haylen::spatial3d::HashGrid>{.index = std::move(grid)});
    resetTables(L, lua_gettop(L));
    return 1;
}

// Creates an octree with `newOctree(box[, {maxEntries = 8, maxDepth = 8, looseness = 1}])`.
int SpatialIndexBinding::newOctree(lua_State* L) {
    haylen::spatial3d::Octree::Settings settings;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kOctreeFields});
        lua::Table::readField(L, 2, "maxEntries", settings.maxEntries);
        lua::Table::readField(L, 2, "maxDepth", settings.maxDepth);
        lua::Table::readField(L, 2, "looseness", settings.looseness);
    }

    haylen::spatial3d::Octree tree(lua::Stack::read<haylen::math::Box>(L, 1), settings);
    lua::Userdata::emplace<ScriptedIndex<haylen::spatial3d::Octree>>(L, ScriptedIndex<haylen::spatial3d::Octree>{.index = std::move(tree)});
    resetTables(L, lua_gettop(L));
    return 1;
}

int SpatialIndexBinding::newAabbTree(lua_State* L) {
    haylen::spatial3d::AabbTree tree = lua_isnoneornil(L, 1) ? haylen::spatial3d::AabbTree() : haylen::spatial3d::AabbTree(lua::Stack::read<float>(L, 1));
    lua::Userdata::emplace<ScriptedIndex<haylen::spatial3d::AabbTree>>(L, ScriptedIndex<haylen::spatial3d::AabbTree>{.index = std::move(tree)});
    resetTables(L, lua_gettop(L));
    return 1;
}

int SpatialIndexBinding::newBvh(lua_State* L) {
    lua::Userdata::emplace<ScriptedIndex<haylen::spatial3d::Bvh>>(L, ScriptedIndex<haylen::spatial3d::Bvh>{});
    resetTables(L, lua_gettop(L));
    return 1;
}

int SpatialIndexBinding::newKdTree(lua_State* L) {
    lua::Userdata::emplace<ScriptedIndex<haylen::spatial3d::KdTree>>(L, ScriptedIndex<haylen::spatial3d::KdTree>{});
    resetTables(L, lua_gettop(L));
    return 1;
}

// Stores a value with `set(value, box)`, or moves it when it is already stored.
template <typename Structure> int SpatialIndexBinding::set(lua_State* L) {
    ScriptedIndex<Structure>& self = lua::Userdata::check<ScriptedIndex<Structure>>(L, 1);
    const haylen::math::Box box = lua::Stack::read<haylen::math::Box>(L, 3);
    store(L, self, [&self, &box](std::uint64_t id) { self.index.set(id, box); });
    return 0;
}

// Stores or moves many values with `setAll(values, buffer)`, where the float buffer holds the smallest and the largest corner of each value in turn.
template <typename Structure> int SpatialIndexBinding::setAll(lua_State* L) {
    ScriptedIndex<Structure>& self = lua::Userdata::check<ScriptedIndex<Structure>>(L, 1);
    storeBatch(L, self, kBoxFloats, [&self] { self.index.setAll(self.ids, self.boxes); });
    return 0;
}

// Stores a value at a point with `set(value, point[, radius])`, or moves it when it is already stored.
int SpatialIndexBinding::setPoint(lua_State* L) {
    ScriptedIndex<haylen::spatial3d::KdTree>& self = lua::Userdata::check<ScriptedIndex<haylen::spatial3d::KdTree>>(L, 1);
    const haylen::math::Vec3 point = lua::Stack::read<haylen::math::Vec3>(L, 3);
    const float radius = lua_isnoneornil(L, 4) ? 0.0F : lua::Stack::read<float>(L, 4);
    store(L, self, [&self, point, radius](std::uint64_t id) { self.index.set(id, point, radius); });
    return 0;
}

// Stores or moves many values with `setAll(values, buffer)`, where the float buffer holds the center and the radius of each value in turn.
int SpatialIndexBinding::setAllPoints(lua_State* L) {
    ScriptedIndex<haylen::spatial3d::KdTree>& self = lua::Userdata::check<ScriptedIndex<haylen::spatial3d::KdTree>>(L, 1);
    storeBatch(L, self, kSphereFloats, [&self] { self.index.setAll(self.ids, self.spheres); });
    return 0;
}

template <typename Structure> int SpatialIndexBinding::remove(lua_State* L) {
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

template <typename Structure> int SpatialIndexBinding::has(lua_State* L) {
    (void)lua::Userdata::check<ScriptedIndex<Structure>>(L, 1);
    lua::Stack::push(L, idOf(L, 2).has_value());
    return 1;
}

template <typename Structure> int SpatialIndexBinding::bounds(lua_State* L) {
    ScriptedIndex<Structure>& self = lua::Userdata::check<ScriptedIndex<Structure>>(L, 1);
    const std::optional<std::uint64_t> id = idOf(L, 2);
    lua::Stack::push(L, id ? self.index.getBounds(*id) : std::nullopt);
    return 1;
}

int SpatialIndexBinding::sphere(lua_State* L) {
    ScriptedIndex<haylen::spatial3d::KdTree>& self = lua::Userdata::check<ScriptedIndex<haylen::spatial3d::KdTree>>(L, 1);
    const std::optional<std::uint64_t> id = idOf(L, 2);
    lua::Stack::push(L, id ? self.index.getSphere(*id) : std::nullopt);
    return 1;
}

template <typename Structure> int SpatialIndexBinding::build(lua_State* L) {
    lua::Userdata::check<ScriptedIndex<Structure>>(L, 1).index.build();
    return 0;
}

template <typename Structure> int SpatialIndexBinding::isBuilt(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedIndex<Structure>>(L, 1).index.isBuilt());
    return 1;
}

int SpatialIndexBinding::refit(lua_State* L) {
    lua::Userdata::check<ScriptedIndex<haylen::spatial3d::Bvh>>(L, 1).index.refit();
    return 0;
}

// Lists the values whose bounds touch the box with `query(box[, results])`.
template <typename Structure> int SpatialIndexBinding::query(lua_State* L) {
    ScriptedIndex<Structure>& self = lua::Userdata::check<ScriptedIndex<Structure>>(L, 1);
    self.index.query(lua::Stack::read<haylen::math::Box>(L, 2), self.ids);
    pushValues(L, self.ids, lua_isnoneornil(L, 3) ? 0 : 3);
    return 1;
}

template <typename Structure> int SpatialIndexBinding::querySphere(lua_State* L) {
    ScriptedIndex<Structure>& self = lua::Userdata::check<ScriptedIndex<Structure>>(L, 1);
    self.index.querySphere(lua::Stack::read<haylen::math::Sphere>(L, 2), self.ids);
    pushValues(L, self.ids, lua_isnoneornil(L, 3) ? 0 : 3);
    return 1;
}

template <typename Structure> int SpatialIndexBinding::queryPoint(lua_State* L) {
    ScriptedIndex<Structure>& self = lua::Userdata::check<ScriptedIndex<Structure>>(L, 1);
    self.index.queryPoint(lua::Stack::read<haylen::math::Vec3>(L, 2), self.ids);
    pushValues(L, self.ids, lua_isnoneornil(L, 3) ? 0 : 3);
    return 1;
}

template <typename Structure> int SpatialIndexBinding::queryFrustum(lua_State* L) {
    ScriptedIndex<Structure>& self = lua::Userdata::check<ScriptedIndex<Structure>>(L, 1);
    self.index.queryFrustum(lua::Userdata::check<haylen::math::Frustum>(L, 2), self.ids);
    pushValues(L, self.ids, lua_isnoneornil(L, 3) ? 0 : 3);
    return 1;
}

// Casts a ray with `raycast(ray[, limit])` and returns the hits from its origin on, each with the value it hit.
template <typename Structure> int SpatialIndexBinding::raycast(lua_State* L) {
    ScriptedIndex<Structure>& self = lua::Userdata::check<ScriptedIndex<Structure>>(L, 1);
    const haylen::math::Ray3D ray = lua::Stack::read<haylen::math::Ray3D>(L, 2);
    const auto limit = lua_isnoneornil(L, 3) ? std::size_t{0} : lua::Stack::read<std::size_t>(L, 3);
    self.index.raycast(ray, limit, self.hits);
    pushHits(L, self.hits);
    return 1;
}

// Finds the closest value with `nearest(point, maxDistance[, accept])`, where `accept(value)` can reject candidates.
template <typename Structure> int SpatialIndexBinding::nearest(lua_State* L) {
    const Structure& structure = lua::Userdata::check<ScriptedIndex<Structure>>(L, 1).index;
    const haylen::math::Vec3 point = lua::Stack::read<haylen::math::Vec3>(L, 2);
    const float maxDistance = lua_isnoneornil(L, 3) ? std::numeric_limits<float>::infinity() : lua::Stack::read<float>(L, 3);
    const bool filtered = !lua_isnoneornil(L, 4);
    if (filtered) {
        luaL_checktype(L, 4, LUA_TFUNCTION);
    }

    // The callback `accept` runs on every candidate from the closest one on, and it may remove values or clear the structure, which replaces the `values` table, so each candidate is looked up in the current table and a value that is gone is skipped.
    std::vector<haylen::spatial3d::Neighbor> candidates;
    structure.nearest(point, filtered ? structure.size() : 1, maxDistance, candidates);

    for (const haylen::spatial3d::Neighbor& candidate : candidates) {
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

        lua_pushvalue(L, 4);
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

// Lists the closest values with `kNearest(point, count[, maxDistance[, results]])`, closest first.
template <typename Structure> int SpatialIndexBinding::nearestList(lua_State* L) {
    ScriptedIndex<Structure>& self = lua::Userdata::check<ScriptedIndex<Structure>>(L, 1);
    const haylen::math::Vec3 point = lua::Stack::read<haylen::math::Vec3>(L, 2);
    const auto count = lua::Stack::read<std::size_t>(L, 3);
    const float maxDistance = lua_isnoneornil(L, 4) ? std::numeric_limits<float>::infinity() : lua::Stack::read<float>(L, 4);
    self.index.nearest(point, count, maxDistance, self.neighbors);

    self.ids.clear();
    for (const haylen::spatial3d::Neighbor& neighbor : self.neighbors) {
        self.ids.push_back(neighbor.id);
    }

    pushValues(L, self.ids, lua_isnoneornil(L, 5) ? 0 : 5);
    return 1;
}

template <typename Structure> int SpatialIndexBinding::clear(lua_State* L) {
    lua::Userdata::check<ScriptedIndex<Structure>>(L, 1).index.clear();
    resetTables(L, 1);
    return 0;
}

template <typename Structure> int SpatialIndexBinding::size(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedIndex<Structure>>(L, 1).index.size());
    return 1;
}

int SpatialIndexBinding::cellSize(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedIndex<haylen::spatial3d::HashGrid>>(L, 1).index.getCellSize());
    return 1;
}

int SpatialIndexBinding::region(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedIndex<haylen::spatial3d::Octree>>(L, 1).index.getRegion());
    return 1;
}

int SpatialIndexBinding::octreeNodeCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedIndex<haylen::spatial3d::Octree>>(L, 1).index.getNodeCount());
    return 1;
}

int SpatialIndexBinding::height(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedIndex<haylen::spatial3d::AabbTree>>(L, 1).index.getHeight());
    return 1;
}

int SpatialIndexBinding::margin(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedIndex<haylen::spatial3d::AabbTree>>(L, 1).index.getMargin());
    return 1;
}

int SpatialIndexBinding::bvhNodeCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedIndex<haylen::spatial3d::Bvh>>(L, 1).index.getNodeCount());
    return 1;
}

int SpatialIndexBinding::depth(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedIndex<haylen::spatial3d::Bvh>>(L, 1).index.getDepth());
    return 1;
}

template <typename Structure> lua::ClassBuilder<ScriptedIndex<Structure>>& SpatialIndexBinding::addQueries(lua::ClassBuilder<ScriptedIndex<Structure>>& builder) {
    return builder.function("remove", &lua::Binding::native<&remove<Structure>>).function("has", &lua::Binding::native<&has<Structure>>).function("query", &lua::Binding::native<&query<Structure>>).function("querySphere", &lua::Binding::native<&querySphere<Structure>>).function("queryPoint", &lua::Binding::native<&queryPoint<Structure>>).function("queryFrustum", &lua::Binding::native<&queryFrustum<Structure>>).function("raycast", &lua::Binding::native<&raycast<Structure>>).function("nearest", &lua::Binding::native<&nearest<Structure>>).function("kNearest", &lua::Binding::native<&nearestList<Structure>>).function("clear", &lua::Binding::native<&clear<Structure>>).property("size", &size<Structure>);
}

void SpatialIndexBinding::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newHashGrid", &lua::Binding::native<&newHashGrid>}, {"newOctree", &lua::Binding::native<&newOctree>}, {"newAabbTree", &lua::Binding::native<&newAabbTree>}, {"newBvh", &lua::Binding::native<&newBvh>}, {"newKdTree", &lua::Binding::native<&newKdTree>}, {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

void SpatialIndexBinding::install(lua_State* L) {
    using haylen::spatial3d::AabbTree;
    using haylen::spatial3d::Bvh;
    using haylen::spatial3d::HashGrid;
    using haylen::spatial3d::KdTree;
    using haylen::spatial3d::Octree;

    lua::ClassBuilder<ScriptedIndex<HashGrid>> hash(L);
    addQueries(hash).function("set", &lua::Binding::native<&set<HashGrid>>).function("setAll", &lua::Binding::native<&setAll<HashGrid>>).function("bounds", &lua::Binding::native<&bounds<HashGrid>>).property("cellSize", &cellSize).install();

    lua::ClassBuilder<ScriptedIndex<Octree>> octree(L);
    addQueries(octree).function("set", &lua::Binding::native<&set<Octree>>).function("setAll", &lua::Binding::native<&setAll<Octree>>).function("bounds", &lua::Binding::native<&bounds<Octree>>).property("region", &region).property("nodeCount", &octreeNodeCount).install();

    lua::ClassBuilder<ScriptedIndex<AabbTree>> aabbTree(L);
    addQueries(aabbTree).function("set", &lua::Binding::native<&set<AabbTree>>).function("setAll", &lua::Binding::native<&setAll<AabbTree>>).function("bounds", &lua::Binding::native<&bounds<AabbTree>>).property("height", &height).property("margin", &margin).install();

    lua::ClassBuilder<ScriptedIndex<Bvh>> bvh(L);
    addQueries(bvh).function("set", &lua::Binding::native<&set<Bvh>>).function("setAll", &lua::Binding::native<&setAll<Bvh>>).function("bounds", &lua::Binding::native<&bounds<Bvh>>).function("build", &lua::Binding::native<&build<Bvh>>).function("refit", &lua::Binding::native<&refit>).property("built", &isBuilt<Bvh>).property("nodeCount", &bvhNodeCount).property("depth", &depth).install();

    lua::ClassBuilder<ScriptedIndex<KdTree>> kdTree(L);
    addQueries(kdTree).function("set", &lua::Binding::native<&setPoint>).function("setAll", &lua::Binding::native<&setAllPoints>).function("sphere", &lua::Binding::native<&sphere>).function("build", &lua::Binding::native<&build<KdTree>>).property("built", &isBuilt<KdTree>).install();
}

} // namespace haylen::lua::spatial3d
