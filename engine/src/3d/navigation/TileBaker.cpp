#include "3d/navigation/TileBaker.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>

#include "3d/navigation/LayerCodec.hpp"

namespace haylen::navigation3d {

TileBaker::TileBaker(const BuildSettings& bakeSettings, const TileGrid& tileGrid) : settings(bakeSettings), grid(tileGrid) {
    const float cellSize = settings.cellSize;
    const float cellHeight = settings.cellHeight;
    slopeDegrees = settings.agentMaxSlope * 180.0F / std::numbers::pi_v<float>;

    config.cs = cellSize;
    config.ch = cellHeight;
    config.walkableSlopeAngle = slopeDegrees;
    config.walkableHeight = static_cast<int>(std::ceil(settings.agentHeight / cellHeight));
    config.walkableClimb = static_cast<int>(std::floor(settings.agentMaxClimb / cellHeight));
    config.walkableRadius = static_cast<int>(std::ceil(settings.agentRadius / cellSize));
    config.maxEdgeLen = static_cast<int>(settings.edgeMaxLength / cellSize);
    config.maxSimplificationError = settings.edgeMaxError / cellSize;
    config.minRegionArea = static_cast<int>(settings.regionMinArea / (cellSize * cellSize));
    config.mergeRegionArea = static_cast<int>(settings.regionMergeArea / (cellSize * cellSize));
    config.maxVertsPerPoly = static_cast<int>(settings.polygonMaxVertices);
    config.tileSize = grid.tileCells;
    config.borderSize = grid.borderCells;
    config.width = grid.tileCells + grid.borderCells * 2;
    config.height = config.width;
    config.detailSampleDist = settings.detailSampleDistance < cellSize * 0.9F ? 0.0F : settings.detailSampleDistance;
    config.detailSampleMaxError = settings.detailSampleMaxError;
}

void TileBaker::failMemory(int x, int z) {
    throw std::runtime_error("The tile " + std::to_string(x) + ", " + std::to_string(z) + " of a navigation mesh did not bake, since the system ran out of memory.");
}

void TileBaker::failSize(int x, int z, std::string_view what) {
    throw std::invalid_argument("The tile " + std::to_string(x) + ", " + std::to_string(z) + " of a navigation mesh holds more " + std::string(what) + " than one tile fits. Bake it with a smaller \"tileSize\" or a larger \"cellSize\".");
}

void TileBaker::require(bool done, int x, int z) {
    if (!done) {
        failMemory(x, z);
    }
}

TileBaker::Column TileBaker::bake(const SourceGeometry& source, std::span<const std::int32_t> triangles, int x, int z) {
    Column column;
    if (triangles.empty()) {
        return column;
    }

    // The corners and areas of the triangles of this column, with the heights they span.
    const std::vector<float>& vertices = source.getVertices();
    const std::vector<std::int32_t>& allCorners = source.getTriangles();
    corners.clear();
    areas.clear();
    float bottom = std::numeric_limits<float>::max();
    float top = std::numeric_limits<float>::lowest();
    for (const std::int32_t triangle : triangles) {
        for (std::size_t corner = 0; corner < 3; ++corner) {
            const std::int32_t vertex = allCorners[static_cast<std::size_t>(triangle) * 3 + corner];
            const float height = vertices[static_cast<std::size_t>(vertex) * 3 + 1];
            bottom = std::min(bottom, height);
            top = std::max(top, height);
            corners.push_back(vertex);
        }
        areas.push_back(source.getTriangleAreas()[static_cast<std::size_t>(triangle)]);
    }

    if ((top - bottom) / settings.cellHeight >= static_cast<float>(RC_SPAN_MAX_HEIGHT)) {
        throw std::invalid_argument("The tile " + std::to_string(x) + ", " + std::to_string(z) + " of a navigation mesh spans more height than 8191 voxels of its \"cellHeight\" hold. Bake it with a larger \"cellHeight\".");
    }

    const math::Box area = grid.getBakeBounds(x, z, settings.cellSize);
    const float minimum[3] = {area.min.x, bottom, area.min.z};
    const float maximum[3] = {area.max.x, top, area.max.z};
    const auto vertexCount = static_cast<int>(vertices.size() / 3);
    const auto triangleCount = static_cast<int>(areas.size());

    const std::unique_ptr<rcHeightfield, decltype(&rcFreeHeightField)> solid(rcAllocHeightfield(), &rcFreeHeightField);
    require(solid != nullptr, x, z);
    require(rcCreateHeightfield(&context, *solid, config.width, config.height, minimum, maximum, config.cs, config.ch), x, z);
    rcClearUnwalkableTriangles(&context, slopeDegrees, vertices.data(), vertexCount, corners.data(), triangleCount, areas.data());
    require(rcRasterizeTriangles(&context, vertices.data(), vertexCount, corners.data(), areas.data(), triangleCount, *solid, config.walkableClimb), x, z);

    if (settings.filterLowObstacles) {
        rcFilterLowHangingWalkableObstacles(&context, config.walkableClimb, *solid);
    }
    if (settings.filterLedges) {
        rcFilterLedgeSpans(&context, config.walkableHeight, config.walkableClimb, *solid);
    }
    if (settings.filterLowCeilings) {
        rcFilterWalkableLowHeightSpans(&context, config.walkableHeight, *solid);
    }

    const std::unique_ptr<rcCompactHeightfield, decltype(&rcFreeCompactHeightfield)> voxels(rcAllocCompactHeightfield(), &rcFreeCompactHeightfield);
    require(voxels != nullptr, x, z);
    require(rcBuildCompactHeightfield(&context, config.walkableHeight, config.walkableClimb, *solid, *voxels), x, z);
    require(rcErodeWalkableArea(&context, config.walkableRadius, *voxels), x, z);
    for (const SourceGeometry::MarkedVolume& volume : source.getVolumes()) {
        rcMarkConvexPolyArea(&context, volume.outline.data(), static_cast<int>(volume.outline.size() / 3), volume.bottom, volume.top, volume.area, *voxels);
    }

    if (settings.obstacles) {
        bakeLayers(*voxels, x, z, column);
    } else {
        bakeMesh(*voxels, x, z, column);
    }

    return column;
}

void TileBaker::bakeMesh(rcCompactHeightfield& voxels, int x, int z, Column& column) {
    switch (settings.partition) {
    case BuildSettings::Partition::Watershed:
        require(rcBuildDistanceField(&context, voxels), x, z);
        if (!rcBuildRegions(&context, voxels, config.borderSize, config.minRegionArea, config.mergeRegionArea)) {
            failSize(x, z, "regions");
        }
        break;
    case BuildSettings::Partition::Monotone:
        if (!rcBuildRegionsMonotone(&context, voxels, config.borderSize, config.minRegionArea, config.mergeRegionArea)) {
            failSize(x, z, "regions");
        }
        break;
    case BuildSettings::Partition::Layers:
        if (!rcBuildLayerRegions(&context, voxels, config.borderSize, config.minRegionArea)) {
            failSize(x, z, "regions");
        }
        break;
    }

    const std::unique_ptr<rcContourSet, decltype(&rcFreeContourSet)> contours(rcAllocContourSet(), &rcFreeContourSet);
    require(contours != nullptr, x, z);
    require(rcBuildContours(&context, voxels, config.maxSimplificationError, config.maxEdgeLen, *contours), x, z);
    if (contours->nconts == 0) {
        return;
    }

    const std::unique_ptr<rcPolyMesh, decltype(&rcFreePolyMesh)> polygons(rcAllocPolyMesh(), &rcFreePolyMesh);
    require(polygons != nullptr, x, z);
    if (!rcBuildPolyMesh(&context, *contours, config.maxVertsPerPoly, *polygons)) {
        failSize(x, z, "polygons");
    }
    if (polygons->npolys == 0) {
        return;
    }

    const std::unique_ptr<rcPolyMeshDetail, decltype(&rcFreePolyMeshDetail)> detail(rcAllocPolyMeshDetail(), &rcFreePolyMeshDetail);
    require(detail != nullptr, x, z);
    require(rcBuildPolyMeshDetail(&context, *polygons, voxels, config.detailSampleDist, config.detailSampleMaxError, *detail), x, z);

    TileMesh& mesh = column.meshes.emplace_back();
    mesh.x = x;
    mesh.z = z;
    std::copy_n(polygons->bmin, 3, mesh.min.begin());
    std::copy_n(polygons->bmax, 3, mesh.max.begin());
    mesh.maxVertices = polygons->nvp;
    const auto vertexCount = static_cast<std::size_t>(polygons->nverts);
    const auto polygonCount = static_cast<std::size_t>(polygons->npolys);
    mesh.vertices.assign(polygons->verts, polygons->verts + vertexCount * 3);
    mesh.polygons.assign(polygons->polys, polygons->polys + polygonCount * 2 * static_cast<std::size_t>(polygons->nvp));
    mesh.areas.assign(polygons->areas, polygons->areas + polygonCount);
    mesh.detailMeshes.assign(detail->meshes, detail->meshes + static_cast<std::size_t>(detail->nmeshes) * 4);
    mesh.detailVertices.assign(detail->verts, detail->verts + static_cast<std::size_t>(detail->nverts) * 3);
    mesh.detailTriangles.assign(detail->tris, detail->tris + static_cast<std::size_t>(detail->ntris) * 4);
}

void TileBaker::bakeLayers(const rcCompactHeightfield& voxels, int x, int z, Column& column) {
    const std::unique_ptr<rcHeightfieldLayerSet, decltype(&rcFreeHeightfieldLayerSet)> floors(rcAllocHeightfieldLayerSet(), &rcFreeHeightfieldLayerSet);
    require(floors != nullptr, x, z);
    if (!rcBuildHeightfieldLayers(&context, voxels, config.borderSize, config.walkableHeight, *floors)) {
        failSize(x, z, "floors");
    }

    for (int index = 0; index < floors->nlayers; ++index) {
        const rcHeightfieldLayer& source = floors->layers[index];
        const auto cells = static_cast<std::size_t>(source.width) * static_cast<std::size_t>(source.height);

        TileLayer& layer = column.layers.emplace_back();
        layer.x = x;
        layer.z = z;
        layer.layer = index;
        std::copy_n(source.bmin, 3, layer.min.begin());
        std::copy_n(source.bmax, 3, layer.max.begin());
        layer.minHeight = static_cast<std::uint16_t>(source.hmin);
        layer.maxHeight = static_cast<std::uint16_t>(source.hmax);
        layer.width = static_cast<std::uint8_t>(source.width);
        layer.height = static_cast<std::uint8_t>(source.height);
        layer.minX = static_cast<std::uint8_t>(source.minx);
        layer.maxX = static_cast<std::uint8_t>(source.maxx);
        layer.minZ = static_cast<std::uint8_t>(source.miny);
        layer.maxZ = static_cast<std::uint8_t>(source.maxy);
        layer.compressed = LayerCodec::compress({source.heights, cells}, {source.areas, cells}, {source.cons, cells});
    }
}

} // namespace haylen::navigation3d
