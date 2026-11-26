#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/core/EnumNames.hpp"
#include "haylen/math/Box.hpp"

namespace haylen::navigation3d {

// How a navigation mesh is baked: the agent it serves, the voxels the geometry rasterizes into and how the walkable voxels become polygons. Every length is in meters and every angle in radians.
struct BuildSettings {
    // How the walkable area of a tile splits into regions before it becomes polygons. `Watershed` makes the most even polygons and is the slowest, `Monotone` is the fastest and makes long thin polygons, and `Layers` sits between them. A mesh with obstacles always splits its tiles into layers, whatever its partition.
    enum class Partition : std::uint8_t {
        Watershed,
        Monotone,
        Layers,
    };

    // The side of a voxel on the ground and its height. Smaller voxels follow the geometry closer and bake slower.
    float cellSize = 0.3F;
    float cellHeight = 0.2F;

    // The agent: the height it needs above the ground, its radius, which keeps the mesh that far from walls, the highest step it climbs and the steepest slope it walks.
    float agentHeight = 2.0F;
    float agentRadius = 0.6F;
    float agentMaxClimb = 0.9F;
    float agentMaxSlope = 0.785398163F;

    // Regions smaller than `regionMinArea` square meters are removed, and regions smaller than `regionMergeArea` merge into larger neighbors.
    float regionMinArea = 6.0F;
    float regionMergeArea = 36.0F;

    // The longest edge of the outline of a polygon, zero for no limit, and how far the outline may stray from the voxels.
    float edgeMaxLength = 12.0F;
    float edgeMaxError = 0.4F;
    std::size_t polygonMaxVertices = 6;

    // The spacing of the samples of the detail mesh, which follows the height of the ground inside each polygon, zero to follow the polygons only, and how far it may stray from the voxels.
    float detailSampleDistance = 1.8F;
    float detailSampleMaxError = 0.2F;

    Partition partition = Partition::Watershed;

    // The side of a tile on the ground. Each tile bakes on its own, so tiles bake in parallel and one tile rebuilds without the others.
    float tileSize = 32.0F;

    // The filters of the voxels: walk over low obstacles such as curbs, remove the ledges an agent would fall from, and remove spans too low for the agent.
    bool filterLowObstacles = true;
    bool filterLedges = true;
    bool filterLowCeilings = true;

    // Keeps the tiles of the mesh as layers, so obstacles carve the mesh at run time, and how many obstacles it holds at once.
    bool obstacles = false;
    std::size_t maxObstacles = 256;

    // The part of the world to bake, all the geometry by default.
    std::optional<math::Box> bounds{};

    static constexpr std::size_t kMinPolygonVertices = 3;
    static constexpr std::size_t kMaxPolygonVertices = 6;
    static constexpr std::size_t kMaxObstacles = 65536;
    static constexpr std::size_t kMaxTileCells = 4096;

    // Throws `std::invalid_argument` with a sentence that names the setting for a value that is not finite, out of its range or too fine for the voxels.
    void validate() const;

    // The side of a tile in voxels.
    [[nodiscard]] int getTileCells() const;

    friend bool operator==(const BuildSettings&, const BuildSettings&) = default;

  private:
    static void requireNonNegative(float value, std::string_view name);
};

} // namespace haylen::navigation3d

namespace haylen::core {

template <> struct EnumNames<navigation3d::BuildSettings::Partition> {
    using Partition = navigation3d::BuildSettings::Partition;

    static constexpr std::array<std::pair<std::string_view, Partition>, 3> kNames{{{"watershed", Partition::Watershed}, {"monotone", Partition::Monotone}, {"layers", Partition::Layers}}};

    static std::optional<Partition> fromName(std::string_view name) {
        const auto found = std::ranges::find(kNames, name, &std::pair<std::string_view, Partition>::first);
        return found != kNames.end() ? std::optional(found->second) : std::nullopt;
    }
    static std::string_view name(Partition value) {
        return std::ranges::find(kNames, value, &std::pair<std::string_view, Partition>::second)->first;
    }
};

} // namespace haylen::core
