#include "3d/navigation/TileAssembler.hpp"

#include <DetourNavMesh.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

#include "3d/navigation/LayerCodec.hpp"
#include "3d/navigation/PolygonValidator.hpp"

namespace haylen::navigation3d {

void TileAssembler::ContourDeleter::operator()(dtTileCacheContourSet* contours) const noexcept {
    dtFreeTileCacheContourSet(allocator, contours);
}

void TileAssembler::PolygonDeleter::operator()(dtTileCachePolyMesh* polygons) const noexcept {
    dtFreeTileCachePolyMesh(allocator, polygons);
}

TileAssembler::TileAssembler(const BuildSettings& bakeSettings) : settings(bakeSettings), climbCells(static_cast<int>(std::floor(bakeSettings.agentMaxClimb / bakeSettings.cellHeight))) {}

void TileAssembler::failAssembly(int x, int z) {
    throw std::runtime_error("The tile " + std::to_string(x) + ", " + std::to_string(z) + " of a navigation mesh did not assemble, since its voxels split into more regions or corners than one tile holds or the system ran out of memory. Bake the mesh again with a smaller \"tileSize\".");
}

void TileAssembler::gatherLinks(std::span<const MeshLink> links) {
    linkPoints.clear();
    linkRadii.clear();
    linkFlags.clear();
    linkAreas.clear();
    linkDirections.clear();
    linkIds.clear();
    for (const MeshLink& link : links) {
        linkPoints.insert(linkPoints.end(), {link.link.start.x, link.link.start.y, link.link.start.z, link.link.end.x, link.link.end.y, link.link.end.z});
        linkRadii.push_back(link.link.radius);
        linkFlags.push_back(1);
        linkAreas.push_back(link.area);
        linkDirections.push_back(link.link.bidirectional ? DT_OFFMESH_CON_BIDIR : 0);
        linkIds.push_back(link.id);
    }
}

TileAssembler::Tile TileAssembler::create(dtNavMeshCreateParams& params, int x, int z) {
    params.offMeshConVerts = linkPoints.data();
    params.offMeshConRad = linkRadii.data();
    params.offMeshConFlags = linkFlags.data();
    params.offMeshConAreas = linkAreas.data();
    params.offMeshConDir = linkDirections.data();
    params.offMeshConUserID = linkIds.data();
    params.offMeshConCount = static_cast<int>(linkIds.size());
    params.walkableHeight = settings.agentHeight;
    params.walkableRadius = settings.agentRadius;
    params.walkableClimb = settings.agentMaxClimb;
    params.cs = settings.cellSize;
    params.ch = settings.cellHeight;
    params.buildBvTree = true;

    // Every polygon walks, and the areas alone tell polygons apart.
    flags.assign(static_cast<std::size_t>(params.polyCount), 1);
    params.polyFlags = flags.data();

    unsigned char* data = nullptr;
    int size = 0;
    if (!dtCreateNavMeshData(&params, &data, &size)) {
        if (params.vertCount >= 0xFFFF) {
            throw std::invalid_argument("The tile " + std::to_string(x) + ", " + std::to_string(z) + " of a navigation mesh holds more vertices than one tile fits. Bake it with a smaller \"tileSize\" or a larger \"cellSize\".");
        }
        failAssembly(x, z);
    }

    Tile tile;
    tile.data.reset(data);
    tile.size = size;
    return tile;
}

TileAssembler::Tile TileAssembler::assemble(const TileMesh& mesh, std::span<const MeshLink> links) {
    gatherLinks(links);

    dtNavMeshCreateParams params{};
    params.verts = mesh.vertices.data();
    params.vertCount = static_cast<int>(mesh.getVertexCount());
    params.polys = mesh.polygons.data();
    params.polyAreas = mesh.areas.data();
    params.polyCount = static_cast<int>(mesh.getPolygonCount());
    params.nvp = mesh.maxVertices;
    params.detailMeshes = mesh.detailMeshes.data();
    params.detailVerts = mesh.detailVertices.data();
    params.detailVertsCount = static_cast<int>(mesh.detailVertices.size() / 3);
    params.detailTris = mesh.detailTriangles.data();
    params.detailTriCount = static_cast<int>(mesh.detailTriangles.size() / 4);
    params.tileX = mesh.x;
    params.tileY = mesh.z;
    params.tileLayer = mesh.layer;
    std::copy(mesh.min.begin(), mesh.min.end(), params.bmin);
    std::copy(mesh.max.begin(), mesh.max.end(), params.bmax);
    return create(params, mesh.x, mesh.z);
}

void TileAssembler::mark(dtTileCacheLayer& voxels, const TileLayer& layer, const Obstacle& obstacle) const {
    // An agent stands in the way of an obstacle when its radius reaches it on the ground and its height reaches the bottom of it, so the obstacle grows by both before it marks the voxels.
    const float reach = settings.agentRadius;
    const float rise = settings.agentHeight;
    if (obstacle.shape == Obstacle::Shape::Cylinder) {
        const float base[3] = {obstacle.position.x, obstacle.position.y - rise, obstacle.position.z};
        dtMarkCylinderArea(voxels, layer.min.data(), settings.cellSize, settings.cellHeight, base, obstacle.radius + reach, obstacle.height + rise, 0);
        return;
    }

    const float center[3] = {obstacle.position.x, obstacle.position.y - rise * 0.5F, obstacle.position.z};
    const float halfExtents[3] = {std::fabs(obstacle.halfExtents.x) + reach, std::fabs(obstacle.halfExtents.y) + rise * 0.5F, std::fabs(obstacle.halfExtents.z) + reach};

    // The turn travels as half the cosine of the yaw and minus half its sine, the form the marking of Detour takes.
    const float turn[2] = {-0.5F * std::sin(obstacle.yaw), 0.5F * std::cos(obstacle.yaw)};
    dtMarkBoxArea(voxels, layer.min.data(), settings.cellSize, settings.cellHeight, center, halfExtents, turn, 0);
}

TileAssembler::Tile TileAssembler::assemble(const TileLayer& layer, std::span<const Obstacle> obstacles, std::span<const MeshLink> links) {
    const std::size_t cells = layer.getCellCount();
    if (!LayerCodec::decompress(layer.compressed, cells * 3, voxelBytes)) {
        failAssembly(layer.x, layer.z);
    }
    regions.assign(cells, 0);

    dtTileCacheLayerHeader header{};
    header.magic = DT_TILECACHE_MAGIC;
    header.version = DT_TILECACHE_VERSION;
    header.tx = layer.x;
    header.ty = layer.z;
    header.tlayer = layer.layer;
    std::copy(layer.min.begin(), layer.min.end(), header.bmin);
    std::copy(layer.max.begin(), layer.max.end(), header.bmax);
    header.hmin = layer.minHeight;
    header.hmax = layer.maxHeight;
    header.width = layer.width;
    header.height = layer.height;
    header.minx = layer.minX;
    header.maxx = layer.maxX;
    header.miny = layer.minZ;
    header.maxy = layer.maxZ;

    dtTileCacheLayer voxels{};
    voxels.header = &header;
    voxels.heights = voxelBytes.data();
    voxels.areas = voxelBytes.data() + cells;
    voxels.cons = voxelBytes.data() + cells * 2;
    voxels.regs = regions.data();
    for (const Obstacle& obstacle : obstacles) {
        mark(voxels, layer, obstacle);
    }

    if (dtStatusFailed(dtBuildTileCacheRegions(&allocator, voxels, climbCells))) {
        failAssembly(layer.x, layer.z);
    }

    const std::unique_ptr<dtTileCacheContourSet, ContourDeleter> contours(dtAllocTileCacheContourSet(&allocator), ContourDeleter{&allocator});
    if (contours == nullptr || dtStatusFailed(dtBuildTileCacheContours(&allocator, voxels, climbCells, settings.edgeMaxError / settings.cellSize, *contours))) {
        failAssembly(layer.x, layer.z);
    }

    const std::unique_ptr<dtTileCachePolyMesh, PolygonDeleter> polygons(dtAllocTileCachePolyMesh(&allocator), PolygonDeleter{&allocator});
    if (polygons == nullptr || dtStatusFailed(dtBuildTileCachePolyMesh(&allocator, *contours, *polygons))) {
        failAssembly(layer.x, layer.z);
    }
    if (polygons->npolys == 0) {
        return {};
    }

    // The areas of the voxels were checked when the layer was read, so the polygons take any area they name.
    const auto polygonCount = static_cast<std::size_t>(polygons->npolys);
    const auto stride = static_cast<std::size_t>(polygons->nvp) * 2;
    const PolygonValidator::Polygons made{.vertices = {polygons->verts, static_cast<std::size_t>(polygons->nverts) * 3}, .polygons = {polygons->polys, polygonCount * stride}, .areas = {polygons->areas, polygonCount}, .maxVertices = polygons->nvp, .areaCount = 0xFF, .width = layer.width, .depth = layer.height};
    if (!PolygonValidator::isValid(made)) {
        throw DamagedLayer("The voxels of the tile " + std::to_string(layer.x) + ", " + std::to_string(layer.z) + " of a navigation mesh make polygons that no path crosses, so the file they came from is damaged.");
    }

    gatherLinks(links);
    dtNavMeshCreateParams params{};
    params.verts = polygons->verts;
    params.vertCount = polygons->nverts;
    params.polys = polygons->polys;
    params.polyAreas = polygons->areas;
    params.polyCount = polygons->npolys;
    params.nvp = polygons->nvp;
    params.tileX = layer.x;
    params.tileY = layer.z;
    params.tileLayer = layer.layer;
    std::copy(layer.min.begin(), layer.min.end(), params.bmin);
    std::copy(layer.max.begin(), layer.max.end(), params.bmax);
    return create(params, layer.x, layer.z);
}

} // namespace haylen::navigation3d
