#pragma once

#include <lua.hpp>

#include <array>
#include <functional>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "haylen/2d/procedural/Delaunay.hpp"
#include "haylen/2d/procedural/Region.hpp"
#include "haylen/2d/procedural/Scatter.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/math/Random.hpp"

namespace haylen::lua {

template <> struct Type<procedural2d::Region> {
    static constexpr const char* name = "haylen.Region";
    using Storage = procedural2d::Region;
};

} // namespace haylen::lua

namespace haylen::procedural2d {

// Installs haylen.procedural2d with regions, scattering, the Delaunay class and Voronoi diagrams, and the map generators of MapGeneratorsLua. Functions that take options read the generator from a random field or seed a new one from a seed field.
class Procedural2DLua final {
  public:
    static void install(lua_State* L);

    // Returns the generator of the options at index: the Random userdata of its random field, which stays on the stack, or local seeded with its seed field.
    [[nodiscard]] static math::Random& readGenerator(lua_State* L, int index, math::Random& local);

    // Reads a Region, a Rect or rectangle table, {center, radius, innerRadius}, {polygon = shape} or a Tiled object table.
    [[nodiscard]] static Region readRegion(lua_State* L, int index);

    // Runs work on the task pool and returns a promise that settles on the frame thread, pushing the result with push.
    template <typename Work, typename Push> static int spawn(lua_State* L, Work&& work, Push push) {
        core::Engine& engine = lua::Runtime::getEngine(L);
        const lua::Promise promise(engine);
        using Value = std::invoke_result_t<Work>;
        // clang-format off
        engine.getJobs().run(std::forward<Work>(work), [promise, push](core::JobSystem::Result<Value> result) {
            if (!result.isOk()) {
                promise.reject(result.error);
                return;
            }
            auto value = std::make_shared<Value>(std::move(*result.value));
            promise.resolveWith([push, value](lua_State* state) { push(state, std::move(*value)); });
        });
        // clang-format on
        promise.push(L);
        return 1;
    }

  private:
    static constexpr std::array<std::string_view, 3> kCircleFields{"center", "radius", "innerRadius"};
    static constexpr std::array<std::string_view, 14> kScatterFields{"region", "method", "density", "spacing", "maximumSpacing", "jitter", "attempts", "densityMap", "exclude", "weights", "biome", "layers", "seed", "random"};
    static constexpr std::array<std::string_view, 3> kLayerFields{"minimum", "maximum", "weights"};
    static constexpr std::array<std::string_view, 4> kNoiseFields{"seed", "frequency", "octaves", "gain"};
    static constexpr std::array<std::string_view, 4> kKinds{"rect", "circle", "ring", "polygon"};

    // Pushes a list of indices counted from 1.
    template <typename Values> static void pushIndices(lua_State* L, const Values& values) {
        lua_createtable(L, static_cast<int>(values.size()), 0);
        for (std::size_t index = 0; index < values.size(); ++index) {
            lua_pushinteger(L, static_cast<lua_Integer>(values[index]) + 1);
            lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
        }
    }

    // Pushes the value kept under name in the user value of the object at index 1, which push builds and pushes the first time.
    template <typename Push> static void pushCached(lua_State* L, const char* name, Push&& push) {
        lua::Userdata::pushField(L, 1, name);
        if (!lua_isnil(L, -1)) {
            return;
        }
        lua_pop(L, 1);
        push();
        lua::Userdata::setField(L, 1, name, -1);
    }

    [[nodiscard]] static Region readTiledObject(lua_State* L, int table);
    // Reads a noise table {seed, frequency, octaves, gain} into a function of fractal noise, mapped from [-1, 1] to [0, 1] when normalized.
    [[nodiscard]] static std::function<float(math::Vec2)> readNoise(lua_State* L, int index, bool normalized);
    // Reads a function field of the options, calling it from Lua, or nothing when absent. Only synchronous calls may use it.
    [[nodiscard]] static std::function<float(math::Vec2)> readCallback(lua_State* L, int index);
    [[nodiscard]] static Scatter::Options readScatter(lua_State* L, int index, bool asynchronous);
    static void pushPoints(lua_State* L, const std::vector<Scatter::Point>& points);

    static int newRegion(lua_State* L);
    static int regionContains(lua_State* L);
    static int regionRandomPoint(lua_State* L);
    static int regionArea(lua_State* L);
    static int regionBounds(lua_State* L);
    static int regionKind(lua_State* L);

    static int scatter(lua_State* L);
    static int scatterAsync(lua_State* L);

    static int delaunay(lua_State* L);
    static int delaunayPoints(lua_State* L);
    static int delaunayTriangles(lua_State* L);
    static int delaunayHalfedges(lua_State* L);
    static int delaunayHull(lua_State* L);
    static int delaunayNeighbors(lua_State* L);
    static int delaunayTriangleCount(lua_State* L);
    static int delaunayCircumcenter(lua_State* L);
    static int delaunayFindNearest(lua_State* L);
    static int voronoi(lua_State* L);
    static int voronoiAsync(lua_State* L);
    static int relax(lua_State* L);
    static int relaxAsync(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::procedural2d
