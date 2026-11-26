#pragma once

#include <Recast.h>

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "3d/navigation/TileGrid.hpp"
#include "3d/navigation/TileLayer.hpp"
#include "3d/navigation/TileMesh.hpp"
#include "haylen/3d/navigation/BuildSettings.hpp"
#include "haylen/3d/navigation/SourceGeometry.hpp"

namespace haylen::navigation3d {

// Bakes the triangles and volumes of a geometry that touch one column of a grid into the polygons of its tile, or into the layers of its floors for a mesh with obstacles. Each baker keeps its own buffers, so bakers on several threads bake columns side by side.
class TileBaker final {
  public:
    struct Column {
        std::vector<TileMesh> meshes;
        std::vector<TileLayer> layers;
    };

    TileBaker(const BuildSettings& bakeSettings, const TileGrid& tileGrid);

    // Throws `std::invalid_argument` for a tile that holds more than its polygons or its floors fit, and `std::runtime_error` when the system runs out of memory.
    [[nodiscard]] Column bake(const SourceGeometry& source, std::span<const std::int32_t> triangles, int x, int z);

  private:
    [[noreturn]] static void failMemory(int x, int z);
    [[noreturn]] static void failSize(int x, int z, std::string_view what);
    static void require(bool done, int x, int z);

    void bakeMesh(rcCompactHeightfield& voxels, int x, int z, Column& column);
    void bakeLayers(const rcCompactHeightfield& voxels, int x, int z, Column& column);

    BuildSettings settings;
    TileGrid grid;
    rcConfig config{};
    float slopeDegrees = 0.0F;
    rcContext context{false};
    std::vector<std::int32_t> corners;
    std::vector<std::uint8_t> areas;
};

} // namespace haylen::navigation3d
