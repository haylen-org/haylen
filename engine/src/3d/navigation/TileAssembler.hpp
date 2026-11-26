#pragma once

#include <DetourAlloc.h>
#include <DetourNavMeshBuilder.h>
#include <DetourTileCacheBuilder.h>

#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>

#include "3d/navigation/MeshLink.hpp"
#include "3d/navigation/TileLayer.hpp"
#include "3d/navigation/TileMesh.hpp"
#include "haylen/3d/navigation/BuildSettings.hpp"
#include "haylen/3d/navigation/Obstacle.hpp"

namespace haylen::navigation3d {

// Turns the stored polygons of a tile, or a layer marked by the obstacles that stand on it, into the data of a tile of Detour, with the links that start in it. Each assembler keeps its own buffers, so assemblers on several threads assemble tiles side by side.
class TileAssembler final {
  public:
    // The data of a tile of Detour, which the navigation mesh frees once it took it.
    struct Tile {
        std::unique_ptr<unsigned char, decltype(&dtFree)> data{nullptr, &dtFree};
        int size = 0;
    };

    // The voxels of a layer that a file brought made polygons that no walk along the surface can cross, so the file is damaged.
    struct DamagedLayer : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    explicit TileAssembler(const BuildSettings& bakeSettings);

    // Returns a tile without data for a layer that the obstacles cover whole. Throws `std::runtime_error` for a layer whose voxels split into more than one tile holds or when the system runs out of memory, and `DamagedLayer` for voxels that make polygons no walk crosses.
    [[nodiscard]] Tile assemble(const TileMesh& mesh, std::span<const MeshLink> links);
    [[nodiscard]] Tile assemble(const TileLayer& layer, std::span<const Obstacle> obstacles, std::span<const MeshLink> links);

  private:
    struct ContourDeleter {
        dtTileCacheAlloc* allocator = nullptr;
        void operator()(dtTileCacheContourSet* contours) const noexcept;
    };
    struct PolygonDeleter {
        dtTileCacheAlloc* allocator = nullptr;
        void operator()(dtTileCachePolyMesh* polygons) const noexcept;
    };

    [[noreturn]] static void failAssembly(int x, int z);

    void gatherLinks(std::span<const MeshLink> links);
    [[nodiscard]] Tile create(dtNavMeshCreateParams& params, int x, int z);
    void mark(dtTileCacheLayer& voxels, const TileLayer& layer, const Obstacle& obstacle) const;

    BuildSettings settings;
    int climbCells = 0;
    std::vector<std::uint16_t> flags;
    std::vector<float> linkPoints;
    std::vector<float> linkRadii;
    std::vector<std::uint16_t> linkFlags;
    std::vector<std::uint8_t> linkAreas;
    std::vector<std::uint8_t> linkDirections;
    std::vector<std::uint32_t> linkIds;
    std::vector<std::uint8_t> voxelBytes;
    std::vector<std::uint8_t> regions;
    dtTileCacheAlloc allocator;
};

} // namespace haylen::navigation3d
