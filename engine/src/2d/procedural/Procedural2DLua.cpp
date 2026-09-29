#include "2d/procedural/Procedural2DLua.hpp"

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>

#include "2d/procedural/MapGeneratorsLua.hpp"
#include "haylen/2d/procedural/Voronoi.hpp"
#include "haylen/2d/tiled/Object.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/EnumNames.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/math/Noise2D.hpp"
#include "math/PolygonLua.hpp"

namespace haylen::lua {

template <> struct Type<procedural2d::Delaunay> {
    static constexpr const char* name = "haylen.Delaunay";
    using Storage = procedural2d::Delaunay;
};

template <> struct EnumNames<procedural2d::Scatter::Method> {
    static std::optional<procedural2d::Scatter::Method> fromName(std::string_view name) {
        if (name == "random") {
            return procedural2d::Scatter::Method::Random;
        }
        if (name == "grid") {
            return procedural2d::Scatter::Method::Grid;
        }
        if (name == "poisson") {
            return procedural2d::Scatter::Method::Poisson;
        }
        return std::nullopt;
    }
    static std::string_view name(procedural2d::Scatter::Method value) {
        switch (value) {
        case procedural2d::Scatter::Method::Grid:
            return "grid";
        case procedural2d::Scatter::Method::Poisson:
            return "poisson";
        case procedural2d::Scatter::Method::Random:
            break;
        }
        return "random";
    }
};

} // namespace haylen::lua

namespace haylen::procedural2d {

math::Random& Procedural2DLua::readGenerator(lua_State* L, int index, math::Random& local) {
    if (lua_isnoneornil(L, index)) {
        return local;
    }
    const int options = lua_absindex(L, index);
    if (lua_getfield(L, options, "random") != LUA_TNIL) {
        return lua::Userdata::check<math::Random>(L, -1);
    }
    lua_pop(L, 1);
    lua_Integer seed = 0;
    lua::Table::readField(L, options, "seed", seed);
    local.reseed(static_cast<std::uint64_t>(seed));
    return local;
}

Region Procedural2DLua::readTiledObject(lua_State* L, int table) {
    tiled::Object object;
    std::string shape;
    lua::Table::readField(L, table, "shape", shape);
    lua::Table::readField(L, table, "x", object.position.x);
    lua::Table::readField(L, table, "y", object.position.y);
    lua::Table::readField(L, table, "width", object.size.x);
    lua::Table::readField(L, table, "height", object.size.y);
    lua::Table::readField(L, table, "rotation", object.rotation);
    lua::Table::readField(L, table, "points", object.points);
    for (int kind = 0; kind <= static_cast<int>(tiled::Object::Shape::Tile); ++kind) {
        if (tiled::Object::shapeName(static_cast<tiled::Object::Shape>(kind)) == shape) {
            object.shape = static_cast<tiled::Object::Shape>(kind);
            return Region::fromObject(object);
        }
    }
    throw std::invalid_argument("Unknown Tiled object shape '" + shape + "'.");
}

Region Procedural2DLua::readRegion(lua_State* L, int index) {
    const int table = lua_absindex(L, index);
    if (const Region* region = lua::Userdata::test<Region>(L, table)) {
        return *region;
    }
    if (const math::Rect* rect = lua::Userdata::test<math::Rect>(L, table)) {
        return Region::rect(*rect);
    }
    luaL_checktype(L, table, LUA_TTABLE);

    if (lua_getfield(L, table, "shape") == LUA_TSTRING) {
        lua_pop(L, 1);
        return readTiledObject(L, table);
    }
    lua_pop(L, 1);
    if (lua_getfield(L, table, "polygon") != LUA_TNIL) {
        Region region = Region::polygon(math::PolygonLua::readShape(L, -1));
        lua_pop(L, 1);
        return region;
    }
    lua_pop(L, 1);
    if (lua_getfield(L, table, "radius") != LUA_TNIL) {
        lua_pop(L, 1);
        lua::Table::checkFields(L, table, {kCircleFields});
        math::Vec2 center{};
        float radius = 0.0F;
        float innerRadius = 0.0F;
        lua::Table::readField(L, table, "center", center);
        lua::Table::readField(L, table, "radius", radius);
        lua::Table::readField(L, table, "innerRadius", innerRadius);
        return Region::ring(center, innerRadius, radius);
    }
    lua_pop(L, 1);
    return Region::rect(lua::Stack::read<math::Rect>(L, table));
}

std::function<float(math::Vec2)> Procedural2DLua::readNoise(lua_State* L, int index, bool normalized) {
    lua::Table::checkFields(L, index, {kNoiseFields});
    lua_Integer seed = 0;
    float frequency = 0.01F;
    int octaves = 4;
    float gain = 0.5F;
    lua::Table::readField(L, index, "seed", seed);
    lua::Table::readField(L, index, "frequency", frequency);
    lua::Table::readField(L, index, "octaves", octaves);
    lua::Table::readField(L, index, "gain", gain);
    // clang-format off
    return [noise = math::Noise2D(static_cast<std::uint64_t>(seed)), frequency, octaves, gain, normalized](math::Vec2 point) {
        const float value = noise.fractal(point.x * frequency, point.y * frequency, octaves, 2.0F, gain);
        return normalized ? (value + 1.0F) * 0.5F : value;
    };
    // clang-format on
}

// The function has to stay on the stack, where the returned callback finds it while the calling binding runs.
std::function<float(math::Vec2)> Procedural2DLua::readCallback(lua_State* L, int index) {
    const int function = lua_absindex(L, index);
    // clang-format off
    return [L, function](math::Vec2 point) {
        lua_pushvalue(L, function);
        lua::Stack::push(L, point);
        lua::Runtime::protectedCall(L, 1, 1);
        const auto value = static_cast<float>(luaL_checknumber(L, -1));
        lua_pop(L, 1);
        return value;
    };
    // clang-format on
}

Scatter::Options Procedural2DLua::readScatter(lua_State* L, int index, bool asynchronous) {
    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kScatterFields});
    Scatter::Options options;
    lua::Table::readField(L, index, "method", options.method);
    lua::Table::readField(L, index, "density", options.density);
    lua::Table::readField(L, index, "spacing", options.spacing);
    lua::Table::readField(L, index, "maximumSpacing", options.maximumSpacing);
    lua::Table::readField(L, index, "jitter", options.jitter);
    lua::Table::readField(L, index, "attempts", options.attempts);
    lua::Table::readField(L, index, "weights", options.weights);

    // Maps are noise tables, or Lua functions of the point outside of asynchronous calls.
    for (const auto& [field, map, normalized] : {std::tuple{"densityMap", &options.densityMap, true}, std::tuple{"biome", &options.biome, false}}) {
        const int type = lua_getfield(L, index, field);
        if (type != LUA_TNIL && type != LUA_TFUNCTION && type != LUA_TTABLE) {
            throw std::invalid_argument(std::string("The ") + field + " option takes a function or a noise table.");
        }
        if (type == LUA_TFUNCTION && asynchronous) {
            throw std::invalid_argument(std::string("Asynchronous scattering takes a noise table for ") + field + " instead of a function.");
        }
        if (type == LUA_TFUNCTION) {
            *map = readCallback(L, -1);
            continue;
        }
        if (type == LUA_TTABLE) {
            *map = readNoise(L, lua_gettop(L), normalized);
        }
        lua_pop(L, 1);
    }

    if (lua_getfield(L, index, "exclude") != LUA_TNIL) {
        luaL_checktype(L, -1, LUA_TTABLE);
        const lua_Integer count = luaL_len(L, -1);
        for (lua_Integer zone = 1; zone <= count; ++zone) {
            lua_rawgeti(L, -1, zone);
            options.exclusions.push_back(readRegion(L, -1));
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    if (lua_getfield(L, index, "layers") != LUA_TNIL) {
        luaL_checktype(L, -1, LUA_TTABLE);
        const lua_Integer count = luaL_len(L, -1);
        for (lua_Integer layer = 1; layer <= count; ++layer) {
            lua_rawgeti(L, -1, layer);
            luaL_checktype(L, -1, LUA_TTABLE);
            lua::Table::checkFields(L, -1, {kLayerFields});
            Scatter::Layer& entry = options.layers.emplace_back();
            lua::Table::readField(L, lua_gettop(L), "minimum", entry.minimum);
            lua::Table::readField(L, lua_gettop(L), "maximum", entry.maximum);
            lua::Table::readField(L, lua_gettop(L), "weights", entry.weights);
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    return options;
}

// Points cross into Lua as {x, y, type} tables, with types counted from 1 like the weights.
void Procedural2DLua::pushPoints(lua_State* L, const std::vector<Scatter::Point>& points) {
    lua_createtable(L, static_cast<int>(points.size()), 0);
    for (std::size_t index = 0; index < points.size(); ++index) {
        lua_createtable(L, 0, 3);
        lua::Stack::push(L, points[index].position.x);
        lua_setfield(L, -2, "x");
        lua::Stack::push(L, points[index].position.y);
        lua_setfield(L, -2, "y");
        lua_pushinteger(L, static_cast<lua_Integer>(points[index].type) + 1);
        lua_setfield(L, -2, "type");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
}

int Procedural2DLua::newRegion(lua_State* L) {
    lua::Userdata::emplace<Region>(L, readRegion(L, 1));
    return 1;
}

int Procedural2DLua::regionContains(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Region>(L, 1).contains(lua::Stack::read<math::Vec2>(L, 2)));
    return 1;
}

int Procedural2DLua::regionRandomPoint(lua_State* L) {
    const Region& region = lua::Userdata::check<Region>(L, 1);
    lua::Stack::push(L, region.getRandomPoint(lua::Userdata::check<math::Random>(L, 2)));
    return 1;
}

int Procedural2DLua::regionArea(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Region>(L, 1).getArea());
    return 1;
}

int Procedural2DLua::regionBounds(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Region>(L, 1).getBounds());
    return 1;
}

int Procedural2DLua::regionKind(lua_State* L) {
    lua::Stack::push(L, kKinds[static_cast<std::size_t>(lua::Userdata::check<Region>(L, 1).getKind())]);
    return 1;
}

int Procedural2DLua::scatter(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_getfield(L, 1, "region");
    const Region region = readRegion(L, -1);
    const Scatter::Options options = readScatter(L, 1, false);
    math::Random local(0);
    math::Random& random = readGenerator(L, 1, local);
    pushPoints(L, Scatter::generate(region, options, random));
    return 1;
}

int Procedural2DLua::scatterAsync(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_getfield(L, 1, "region");
    Region region = readRegion(L, -1);
    Scatter::Options options = readScatter(L, 1, true);
    math::Random local(0);
    math::Random random = readGenerator(L, 1, local);
    // clang-format off
    return spawn(L, [region = std::move(region), options = std::move(options), random]() mutable {
        return Scatter::generate(region, options, random);
    }, [](lua_State* state, const std::vector<Scatter::Point>& points) { pushPoints(state, points); });
    // clang-format on
}

// Triangulates a list of points with delaunay(points) into a Delaunay object, whose lists count points and edges from 1.
int Procedural2DLua::delaunay(lua_State* L) {
    auto points = lua::Stack::read<std::vector<math::Vec2>>(L, 1);
    lua::Userdata::emplace<Delaunay>(L, std::move(points));
    return 1;
}

int Procedural2DLua::delaunayPoints(lua_State* L) {
    const Delaunay& triangulation = lua::Userdata::check<Delaunay>(L, 1);
    pushCached(L, "points", [L, &triangulation] { lua::Stack::push(L, triangulation.getPoints()); });
    return 1;
}

int Procedural2DLua::delaunayTriangles(lua_State* L) {
    const Delaunay& triangulation = lua::Userdata::check<Delaunay>(L, 1);
    pushCached(L, "triangles", [L, &triangulation] { pushIndices(L, triangulation.getTriangles()); });
    return 1;
}

// Edges on the hull have no twin, so their halfedge is 0.
int Procedural2DLua::delaunayHalfedges(lua_State* L) {
    const Delaunay& triangulation = lua::Userdata::check<Delaunay>(L, 1);
    pushCached(L, "halfedges", [L, &triangulation] { pushIndices(L, triangulation.getHalfedges()); });
    return 1;
}

int Procedural2DLua::delaunayHull(lua_State* L) {
    const Delaunay& triangulation = lua::Userdata::check<Delaunay>(L, 1);
    pushCached(L, "hull", [L, &triangulation] { pushIndices(L, triangulation.getHull()); });
    return 1;
}

int Procedural2DLua::delaunayNeighbors(lua_State* L) {
    const Delaunay& triangulation = lua::Userdata::check<Delaunay>(L, 1);
    // clang-format off
    pushCached(L, "neighbors", [L, &triangulation] {
        const auto& neighbors = triangulation.getNeighbors();
        lua_createtable(L, static_cast<int>(neighbors.size()), 0);
        for (std::size_t point = 0; point < neighbors.size(); ++point) {
            pushIndices(L, neighbors[point]);
            lua_rawseti(L, -2, static_cast<lua_Integer>(point + 1));
        }
    });
    // clang-format on
    return 1;
}

int Procedural2DLua::delaunayTriangleCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Delaunay>(L, 1).getTriangleCount());
    return 1;
}

int Procedural2DLua::delaunayCircumcenter(lua_State* L) {
    const Delaunay& triangulation = lua::Userdata::check<Delaunay>(L, 1);
    const auto triangle = lua::Stack::read<lua_Integer>(L, 2);
    luaL_argcheck(L, triangle >= 1 && static_cast<std::size_t>(triangle) <= triangulation.getTriangleCount(), 2, "the triangle is outside the triangulation");
    lua::Stack::push(L, triangulation.getCircumcenter(static_cast<std::size_t>(triangle - 1)));
    return 1;
}

// Finds the point nearest to a position with findNearest(position[, start]), walking from the point start, or returns nil without points.
int Procedural2DLua::delaunayFindNearest(lua_State* L) {
    const Delaunay& triangulation = lua::Userdata::check<Delaunay>(L, 1);
    const auto position = lua::Stack::read<math::Vec2>(L, 2);
    const std::size_t count = triangulation.getPoints().size();
    if (count == 0) {
        lua_pushnil(L);
        return 1;
    }
    if (lua_isnoneornil(L, 3)) {
        lua::Stack::push(L, triangulation.findNearest(position) + 1);
        return 1;
    }

    const auto start = lua::Stack::read<lua_Integer>(L, 3);
    luaL_argcheck(L, start >= 1 && static_cast<std::size_t>(start) <= count, 3, "the start is outside the points");
    lua::Stack::push(L, triangulation.findNearest(position, static_cast<std::uint32_t>(start - 1)) + 1);
    return 1;
}

int Procedural2DLua::voronoi(lua_State* L) {
    const Voronoi diagram(lua::Stack::read<std::vector<math::Vec2>>(L, 1), lua::Stack::read<math::Rect>(L, 2));
    lua::Stack::push(L, diagram.getCells());
    return 1;
}

int Procedural2DLua::voronoiAsync(lua_State* L) {
    auto points = lua::Stack::read<std::vector<math::Vec2>>(L, 1);
    const auto bounds = lua::Stack::read<math::Rect>(L, 2);
    // clang-format off
    return spawn(L, [points = std::move(points), bounds]() mutable {
        return Voronoi(std::move(points), bounds).getCells();
    }, [](lua_State* state, const std::vector<std::vector<math::Vec2>>& cells) { lua::Stack::push(state, cells); });
    // clang-format on
}

int Procedural2DLua::relax(lua_State* L) {
    lua::Stack::push(L, Voronoi::relax(lua::Stack::read<std::vector<math::Vec2>>(L, 1), lua::Stack::read<math::Rect>(L, 2), static_cast<int>(luaL_optinteger(L, 3, 1))));
    return 1;
}

int Procedural2DLua::relaxAsync(lua_State* L) {
    auto points = lua::Stack::read<std::vector<math::Vec2>>(L, 1);
    const auto bounds = lua::Stack::read<math::Rect>(L, 2);
    const auto iterations = static_cast<int>(luaL_optinteger(L, 3, 1));
    // clang-format off
    return spawn(L, [points = std::move(points), bounds, iterations]() mutable {
        return Voronoi::relax(std::move(points), bounds, iterations);
    }, [](lua_State* state, const std::vector<math::Vec2>& sites) { lua::Stack::push(state, sites); });
    // clang-format on
}

int Procedural2DLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"region", &lua::Binding::native<&newRegion>}, {"scatter", &lua::Binding::native<&scatter>}, {"scatterAsync", &lua::Binding::native<&scatterAsync>}, {"delaunay", &lua::Binding::native<&delaunay>}, {"voronoi", &lua::Binding::native<&voronoi>}, {"voronoiAsync", &lua::Binding::native<&voronoiAsync>}, {"relax", &lua::Binding::native<&relax>}, {"relaxAsync", &lua::Binding::native<&relaxAsync>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    MapGeneratorsLua::addFunctions(L);
    return 1;
}

void Procedural2DLua::install(lua_State* L) {
    lua::ClassBuilder<Region>(L).function("contains", &lua::Binding::native<&regionContains>).function("randomPoint", &lua::Binding::native<&regionRandomPoint>).property("area", &regionArea).property("bounds", &regionBounds).property("kind", &regionKind).install();
    lua::ClassBuilder<Delaunay>(L).function("circumcenter", &lua::Binding::native<&delaunayCircumcenter>).function("findNearest", &lua::Binding::native<&delaunayFindNearest>).property("points", &delaunayPoints).property("triangles", &delaunayTriangles).property("halfedges", &delaunayHalfedges).property("hull", &delaunayHull).property("neighbors", &delaunayNeighbors).property("triangleCount", &delaunayTriangleCount).install();
    MapGeneratorsLua::install(L);
    lua::Binding::preload(L, "haylen.procedural2d", &open);
}

} // namespace haylen::procedural2d
