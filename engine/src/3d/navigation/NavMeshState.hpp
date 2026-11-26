#pragma once

#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "3d/navigation/MeshLink.hpp"
#include "3d/navigation/NavMeshFile.hpp"
#include "3d/navigation/RandomPoints.hpp"
#include "3d/navigation/TileAssembler.hpp"
#include "3d/navigation/TileGrid.hpp"
#include "3d/navigation/TileIndex.hpp"
#include "3d/navigation/TileLayer.hpp"
#include "3d/navigation/TileMesh.hpp"
#include "haylen/3d/navigation/NavMesh.hpp"

namespace haylen::navigation3d {

// What a navigation mesh owns: its settings and grid, the stored tiles of every column with the tiles of Detour made from them, its links and obstacles, the columns that wait for a rebuild and the query of the frame thread.
struct NavMesh::State : std::enable_shared_from_this<NavMesh::State> {
    // A column of the grid: what it stores, the tiles of Detour in place, the geometry of a bake that waits for it, and whether it waits for a rebuild or one runs.
    struct Column {
        std::vector<TileMesh> meshes;
        std::vector<TileLayer> layers;
        std::vector<dtTileRef> tiles;
        std::shared_ptr<const SourceGeometry> source;
        bool dirty = false;
        bool building = false;
    };

    // What the rebuild of a column reads, copied so it runs on a worker while the mesh changes.
    struct Work {
        int x = 0;
        int z = 0;
        std::shared_ptr<const SourceGeometry> source;
        std::vector<TileMesh> meshes;
        std::vector<TileLayer> layers;
        std::vector<MeshLink> links;
        std::vector<Obstacle> obstacles;
    };

    // What the rebuild of a column made: the stored tiles of a new bake, and the tiles of Detour.
    struct Built {
        bool baked = false;
        std::vector<TileMesh> meshes;
        std::vector<TileLayer> layers;
        std::vector<TileAssembler::Tile> tiles;
    };

    // The bake that every column shares: the first job indexes the geometry and posts a job for each column, and the job of the last column to finish makes the mesh.
    struct Bake {
        core::JobSystem* jobs = nullptr;
        std::shared_ptr<const SourceGeometry> source;
        std::unique_ptr<State> state;
        std::unique_ptr<TileIndex> index;
        std::atomic<std::size_t> remaining{0};
        std::mutex mutex;
        std::string error;
        BuildCallback callback;
    };

    static constexpr std::size_t kLayerHeadroom = 4;
    static constexpr std::size_t kMaxQueryNodes = 65534;

    BuildSettings settings;
    TileGrid grid;
    std::vector<std::string> areas;
    std::unique_ptr<dtNavMesh, decltype(&dtFreeNavMesh)> mesh{nullptr, &dtFreeNavMesh};
    std::unique_ptr<dtNavMeshQuery, decltype(&dtFreeNavMeshQuery)> query{nullptr, &dtFreeNavMeshQuery};
    std::size_t queryNodes = 0;
    std::vector<Column> columns;
    std::map<std::uint32_t, MeshLink> links;
    std::uint32_t nextLink = 1;
    std::map<std::uint32_t, Obstacle> obstacles;
    std::uint32_t nextObstacle = 1;
    std::size_t pending = 0;
    std::vector<std::function<void(std::string)>> waiters;
    std::vector<std::size_t> dirtyColumns;

    // The buffers of the queries of the frame thread, which grow to the largest query and then stay.
    std::vector<dtPolyRef> polygons;
    std::vector<float> points;
    std::vector<unsigned char> pointFlags;
    std::vector<dtPolyRef> pointPolygons;

    RandomPoints::Buffers randomBuffers;

    // Bakes every column of the geometry, one after the other or in jobs, and makes the mesh from what they made.
    [[nodiscard]] static std::unique_ptr<State> bake(const SourceGeometry& source, const BuildSettings& settings);
    static void bakeAsync(core::JobSystem& jobs, std::shared_ptr<const SourceGeometry> source, const BuildSettings& settings, BuildCallback callback);
    static void indexBake(const std::shared_ptr<Bake>& bake);
    static void bakeColumn(const std::shared_ptr<Bake>& bake, std::size_t index);
    static void deliverBake(const std::shared_ptr<Bake>& bake);
    [[nodiscard]] static std::unique_ptr<State> make(const NavMeshFile::Contents& contents);
    [[nodiscard]] static Built buildColumn(const BuildSettings& settings, const TileGrid& grid, Work work);

    [[nodiscard]] NavMeshFile::Contents getContents() const;
    [[nodiscard]] std::uint8_t getArea(std::string_view name) const;
    void addSourceLinks(const SourceGeometry& source);
    [[nodiscard]] std::vector<MeshLink> collectLinks(int x, int z) const;
    [[nodiscard]] std::vector<Obstacle> collectObstacles(int x, int z) const;

    // Starts Detour with room for the tiles the columns store and the layers that rebuilds may add.
    void start();
    void place(std::size_t column, Built built);
    [[nodiscard]] Work prepare(std::size_t column);
    void finish(std::size_t column, core::JobSystem::Result<Built> result);
    void markColumns(const math::Box& area, const std::shared_ptr<const SourceGeometry>& source = {});
    void markColumn(std::size_t index);
    void markLinkColumn(const Link& link);
    [[nodiscard]] math::Box getObstacleBounds(const Obstacle& obstacle) const noexcept;
    [[nodiscard]] dtNavMeshQuery& getQuery(std::size_t maxNodes);

    // The location of a point on a polygon, with the area of the polygon.
    [[nodiscard]] Location locate(dtPolyRef polygon, const std::array<float, 3>& position) const;
};

} // namespace haylen::navigation3d
