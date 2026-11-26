#pragma once

#include <array>
#include <optional>
#include <string_view>

#include "haylen/3d/navigation/BuildSettings.hpp"
#include "haylen/3d/navigation/Crowd.hpp"
#include "haylen/3d/navigation/Link.hpp"
#include "haylen/3d/navigation/NavMesh.hpp"
#include "haylen/3d/navigation/Obstacle.hpp"
#include "haylen/math/Random.hpp"

struct lua_State;

namespace haylen::lua::navigation3d {

// Reads the option tables of `haylen.navigation3d` and pushes the tables of its answers, with the names of their areas.
class NavigationOptions final {
  public:
    static constexpr std::array<std::string_view, 20> kSettingFields{"cellSize", "cellHeight", "agentHeight", "agentRadius", "agentMaxClimb", "agentMaxSlope", "regionMinArea", "regionMergeArea", "edgeMaxLength", "edgeMaxError", "polygonMaxVertices", "detailSampleDistance", "detailSampleMaxError", "partition", "tileSize", "filterLowObstacles", "filterLedges", "filterLowCeilings", "obstacles", "maxObstacles"};
    static constexpr std::array<std::string_view, 1> kBoundsField{"bounds"};
    static constexpr std::array<std::string_view, 5> kQueryFields{"filter", "searchExtents", "maxNodes", "maxPolygons", "maxPoints"};
    static constexpr std::array<std::string_view, 2> kRandomFields{"random", "seed"};
    static constexpr std::array<std::string_view, 4> kLinkFields{"radius", "bidirectional", "kind", "area"};
    static constexpr std::array<std::string_view, 6> kObstacleFields{"shape", "position", "radius", "height", "halfExtents", "yaw"};
    static constexpr std::array<std::string_view, 3> kCrowdFields{"maxAgents", "maxAgentRadius", "pathBudget"};
    static constexpr std::array<std::string_view, 17> kAgentFields{"position", "radius", "height", "maxSpeed", "maxAcceleration", "collisionRange", "pathOptimizationRange", "separationWeight", "avoidance", "anticipateTurns", "optimizeVisibility", "optimizeTopology", "separation", "filter", "linkTraversal", "jumpHeight", "climbSpeed"};

    [[nodiscard]] static haylen::navigation3d::BuildSettings readSettings(lua_State* L, int index);
    [[nodiscard]] static haylen::navigation3d::NavMesh::QueryOptions readQuery(lua_State* L, int index, std::initializer_list<std::span<const std::string_view>> extras = {});

    // Reads the generator of a random pick: the `random` of the options, which advances, or a new one from their `seed`, 0 by default.
    [[nodiscard]] static haylen::math::Random& readRandom(lua_State* L, int index, haylen::math::Random& own);
    [[nodiscard]] static haylen::navigation3d::Link readLink(lua_State* L, int start, int end, int options);
    [[nodiscard]] static haylen::navigation3d::Obstacle readObstacle(lua_State* L, int index);
    [[nodiscard]] static haylen::navigation3d::Crowd::Options readCrowd(lua_State* L, int index);

    static void pushLocation(lua_State* L, const haylen::navigation3d::NavMesh& mesh, const std::optional<haylen::navigation3d::NavMesh::Location>& location);
    static void pushLink(lua_State* L, const haylen::navigation3d::Link& link);
    static void pushObstacle(lua_State* L, const haylen::navigation3d::Obstacle& obstacle);
    static void pushSettings(lua_State* L, const haylen::navigation3d::BuildSettings& settings);
    static void pushAreaName(lua_State* L, const haylen::navigation3d::NavMesh& mesh, std::uint8_t area);
};

} // namespace haylen::lua::navigation3d
