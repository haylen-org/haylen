#include "3d/navigation/NavigationOptions.hpp"

#include <lua.hpp>

#include <cstdint>
#include <stdexcept>
#include <utility>

#include "3d/navigation/QueryFilterBinding.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua::navigation3d {

haylen::navigation3d::BuildSettings NavigationOptions::readSettings(lua_State* L, int index) {
    haylen::navigation3d::BuildSettings settings;
    if (lua_isnoneornil(L, index)) {
        return settings;
    }

    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kSettingFields, kBoundsField});
    lua::Table::readField(L, index, "cellSize", settings.cellSize);
    lua::Table::readField(L, index, "cellHeight", settings.cellHeight);
    lua::Table::readField(L, index, "agentHeight", settings.agentHeight);
    lua::Table::readField(L, index, "agentRadius", settings.agentRadius);
    lua::Table::readField(L, index, "agentMaxClimb", settings.agentMaxClimb);
    lua::Table::readField(L, index, "agentMaxSlope", settings.agentMaxSlope);
    lua::Table::readField(L, index, "regionMinArea", settings.regionMinArea);
    lua::Table::readField(L, index, "regionMergeArea", settings.regionMergeArea);
    lua::Table::readField(L, index, "edgeMaxLength", settings.edgeMaxLength);
    lua::Table::readField(L, index, "edgeMaxError", settings.edgeMaxError);
    lua::Table::readField(L, index, "polygonMaxVertices", settings.polygonMaxVertices);
    lua::Table::readField(L, index, "detailSampleDistance", settings.detailSampleDistance);
    lua::Table::readField(L, index, "detailSampleMaxError", settings.detailSampleMaxError);
    lua::Table::readField(L, index, "partition", settings.partition);
    lua::Table::readField(L, index, "tileSize", settings.tileSize);
    lua::Table::readField(L, index, "filterLowObstacles", settings.filterLowObstacles);
    lua::Table::readField(L, index, "filterLedges", settings.filterLedges);
    lua::Table::readField(L, index, "filterLowCeilings", settings.filterLowCeilings);
    lua::Table::readField(L, index, "obstacles", settings.obstacles);
    lua::Table::readField(L, index, "maxObstacles", settings.maxObstacles);

    lua_getfield(L, index, "bounds");
    if (!lua_isnil(L, -1)) {
        haylen::math::Box bounds;
        lua::Table::readValue(L, "bounds", bounds);
        settings.bounds = bounds;
    }
    lua_pop(L, 1);

    settings.validate();
    return settings;
}

haylen::navigation3d::NavMesh::QueryOptions NavigationOptions::readQuery(lua_State* L, int index, std::initializer_list<std::span<const std::string_view>> extras) {
    haylen::navigation3d::NavMesh::QueryOptions options;
    if (lua_isnoneornil(L, index)) {
        return options;
    }

    luaL_checktype(L, index, LUA_TTABLE);
    // clang-format off
    lua::Table::readFields(L, index, kQueryFields, extras, [L, &options](std::string_view key) {
        if (key == "filter") {
            options.filter = &lua::Userdata::check<ScriptedFilter>(L, -1).filter;
        } else if (key == "searchExtents") {
            lua::Table::readValue(L, key, options.searchExtents);
        } else if (key == "maxNodes") {
            lua::Table::readValue(L, key, options.maxNodes);
        } else if (key == "maxPolygons") {
            lua::Table::readValue(L, key, options.maxPolygons);
        } else {
            lua::Table::readValue(L, key, options.maxPoints);
        }
    });
    // clang-format on

    if (!options.searchExtents.isFinite() || options.searchExtents.x < 0.0F || options.searchExtents.y < 0.0F || options.searchExtents.z < 0.0F) {
        throw std::invalid_argument("The option \"searchExtents\" of a query of a navigation mesh must be a vector of finite numbers of at least zero.");
    }

    return options;
}

haylen::math::Random& NavigationOptions::readRandom(lua_State* L, int index, haylen::math::Random& own) {
    if (lua_isnoneornil(L, index)) {
        own = haylen::math::Random(0);
        return own;
    }

    lua_getfield(L, index, "random");
    haylen::math::Random* random = lua_isnil(L, -1) ? nullptr : &lua::Userdata::check<haylen::math::Random>(L, -1);
    lua_pop(L, 1);
    if (random != nullptr) {
        return *random;
    }

    lua_Integer seed = 0;
    lua::Table::readField(L, index, "seed", seed);
    own = haylen::math::Random(static_cast<std::uint64_t>(seed));
    return own;
}

haylen::navigation3d::Link NavigationOptions::readLink(lua_State* L, int start, int end, int options) {
    haylen::navigation3d::Link link{.start = lua::Stack::read<haylen::math::Vec3>(L, start), .end = lua::Stack::read<haylen::math::Vec3>(L, end)};
    if (lua_isnoneornil(L, options)) {
        return link;
    }

    luaL_checktype(L, options, LUA_TTABLE);
    lua::Table::checkFields(L, options, {kLinkFields});
    lua::Table::readField(L, options, "radius", link.radius);
    lua::Table::readField(L, options, "bidirectional", link.bidirectional);
    lua::Table::readField(L, options, "kind", link.kind);
    lua::Table::readField(L, options, "area", link.area);
    return link;
}

haylen::navigation3d::Obstacle NavigationOptions::readObstacle(lua_State* L, int index) {
    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kObstacleFields});
    haylen::navigation3d::Obstacle obstacle;
    lua::Table::readField(L, index, "shape", obstacle.shape);
    lua::Table::readField(L, index, "position", obstacle.position);
    lua::Table::readField(L, index, "radius", obstacle.radius);
    lua::Table::readField(L, index, "height", obstacle.height);
    lua::Table::readField(L, index, "halfExtents", obstacle.halfExtents);
    lua::Table::readField(L, index, "yaw", obstacle.yaw);
    return obstacle;
}

haylen::navigation3d::Crowd::Options NavigationOptions::readCrowd(lua_State* L, int index) {
    haylen::navigation3d::Crowd::Options options;
    if (lua_isnoneornil(L, index)) {
        return options;
    }

    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kCrowdFields});
    lua::Table::readField(L, index, "maxAgents", options.maxAgents);
    lua::Table::readField(L, index, "maxAgentRadius", options.maxAgentRadius);
    lua::Table::readField(L, index, "pathBudget", options.pathBudget);
    return options;
}

void NavigationOptions::pushAreaName(lua_State* L, const haylen::navigation3d::NavMesh& mesh, std::uint8_t area) {
    if (area == 0 || area > mesh.getAreas().size()) {
        lua::Stack::push(L, std::string_view("none"));
        return;
    }

    lua::Stack::push(L, std::string_view(mesh.getAreas()[area - 1U]));
}

void NavigationOptions::pushLocation(lua_State* L, const haylen::navigation3d::NavMesh& mesh, const std::optional<haylen::navigation3d::NavMesh::Location>& location) {
    if (!location) {
        lua_pushnil(L);
        return;
    }

    lua_createtable(L, 0, 3);
    lua::Stack::push(L, location->position);
    lua_setfield(L, -2, "position");
    lua_pushinteger(L, static_cast<lua_Integer>(location->polygon));
    lua_setfield(L, -2, "polygon");
    pushAreaName(L, mesh, location->area);
    lua_setfield(L, -2, "area");
}

void NavigationOptions::pushLink(lua_State* L, const haylen::navigation3d::Link& link) {
    lua_createtable(L, 0, 6);
    lua::Stack::push(L, link.start);
    lua_setfield(L, -2, "start");
    lua::Stack::push(L, link.end);
    lua_setfield(L, -2, "end");
    lua::Stack::push(L, link.radius);
    lua_setfield(L, -2, "radius");
    lua::Stack::push(L, link.bidirectional);
    lua_setfield(L, -2, "bidirectional");
    lua::Stack::push(L, link.kind);
    lua_setfield(L, -2, "kind");
    lua::Stack::push(L, std::string_view(link.area));
    lua_setfield(L, -2, "area");
}

void NavigationOptions::pushObstacle(lua_State* L, const haylen::navigation3d::Obstacle& obstacle) {
    lua_createtable(L, 0, 6);
    lua::Stack::push(L, obstacle.shape);
    lua_setfield(L, -2, "shape");
    lua::Stack::push(L, obstacle.position);
    lua_setfield(L, -2, "position");
    if (obstacle.shape == haylen::navigation3d::Obstacle::Shape::Cylinder) {
        lua::Stack::push(L, obstacle.radius);
        lua_setfield(L, -2, "radius");
        lua::Stack::push(L, obstacle.height);
        lua_setfield(L, -2, "height");
        return;
    }

    lua::Stack::push(L, obstacle.halfExtents);
    lua_setfield(L, -2, "halfExtents");
    lua::Stack::push(L, obstacle.yaw);
    lua_setfield(L, -2, "yaw");
}

void NavigationOptions::pushSettings(lua_State* L, const haylen::navigation3d::BuildSettings& settings) {
    lua_createtable(L, 0, 21);
    const std::array<std::pair<const char*, float>, 13> numbers{{{"cellSize", settings.cellSize}, {"cellHeight", settings.cellHeight}, {"agentHeight", settings.agentHeight}, {"agentRadius", settings.agentRadius}, {"agentMaxClimb", settings.agentMaxClimb}, {"agentMaxSlope", settings.agentMaxSlope}, {"regionMinArea", settings.regionMinArea}, {"regionMergeArea", settings.regionMergeArea}, {"edgeMaxLength", settings.edgeMaxLength}, {"edgeMaxError", settings.edgeMaxError}, {"detailSampleDistance", settings.detailSampleDistance}, {"detailSampleMaxError", settings.detailSampleMaxError}, {"tileSize", settings.tileSize}}};
    for (const auto& [name, value] : numbers) {
        lua::Stack::push(L, value);
        lua_setfield(L, -2, name);
    }

    lua_pushinteger(L, static_cast<lua_Integer>(settings.polygonMaxVertices));
    lua_setfield(L, -2, "polygonMaxVertices");
    lua_pushinteger(L, static_cast<lua_Integer>(settings.maxObstacles));
    lua_setfield(L, -2, "maxObstacles");
    lua::Stack::push(L, settings.partition);
    lua_setfield(L, -2, "partition");
    lua::Stack::push(L, settings.filterLowObstacles);
    lua_setfield(L, -2, "filterLowObstacles");
    lua::Stack::push(L, settings.filterLedges);
    lua_setfield(L, -2, "filterLedges");
    lua::Stack::push(L, settings.filterLowCeilings);
    lua_setfield(L, -2, "filterLowCeilings");
    lua::Stack::push(L, settings.obstacles);
    lua_setfield(L, -2, "obstacles");
    if (settings.bounds) {
        lua::Stack::push(L, *settings.bounds);
        lua_setfield(L, -2, "bounds");
    }
}

} // namespace haylen::lua::navigation3d
