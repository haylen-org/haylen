#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "3d/navigation/MeshLink.hpp"
#include "3d/navigation/TileGrid.hpp"
#include "3d/navigation/TileLayer.hpp"
#include "3d/navigation/TileMesh.hpp"
#include "haylen/3d/navigation/BuildSettings.hpp"
#include "haylen/math/Box.hpp"

namespace haylen::content {
class BinaryReader;
class BinaryWriter;
} // namespace haylen::content

namespace haylen::navigation3d {

// The `.navmesh` format: the settings and bounds of the bake, the names of the areas, the links and the stored tiles of a navigation mesh, in little-endian order after the magic `HNAV` and the version. Its bytes may be hostile, so the reader checks every count against the bytes that remain and every index, neighbor, area, detail triangle and layer voxel against what it names, with the placement of every tile, the shape of every polygon and the reach of every point that the queries of Detour count on, before any of it reaches Detour, and fails with the sentence that the file is damaged or was not made by this version of Haylen.
class NavMeshFile final {
  public:
    struct Contents {
        BuildSettings settings;
        math::Box bounds;
        std::vector<std::string> areas;
        std::vector<MeshLink> links;
        std::vector<TileMesh> meshes;
        std::vector<TileLayer> layers;
    };

    static constexpr std::uint32_t kMagic = 0x56414E48;
    static constexpr std::uint32_t kVersion = 1;
    static constexpr std::size_t kMaxAreaName = 256;

    // The flag of each of the three edges of a detail triangle that lie on the outline of its polygon, two bits apart.
    static constexpr std::uint8_t kOutlineEdges = 0x15;

    [[nodiscard]] static std::vector<std::uint8_t> encode(const Contents& contents);

    // Throws `std::runtime_error` that names the file for bytes that break any rule of the format.
    [[nodiscard]] static Contents decode(std::span<const std::uint8_t> bytes, std::string_view file);

    // The sentence of a file that is damaged or was not made by this version of Haylen, for the bytes that the reader refuses and for layers whose voxels make polygons that no path crosses.
    [[nodiscard]] static std::string describeDamage(std::string_view file);

  private:
    [[nodiscard]] static std::string getSubject(std::string_view file);

    static void writeFloat(content::BinaryWriter& writer, float value);
    static void writeVector(content::BinaryWriter& writer, const std::array<float, 3>& value);
    static void writeSettings(content::BinaryWriter& writer, const BuildSettings& settings);
    static void writeMesh(content::BinaryWriter& writer, const TileMesh& mesh);
    static void writeLayer(content::BinaryWriter& writer, const TileLayer& layer);

    [[nodiscard]] static float readFloat(content::BinaryReader& reader);
    [[nodiscard]] static float readFinite(content::BinaryReader& reader);
    [[nodiscard]] static float readCoordinate(content::BinaryReader& reader);
    [[nodiscard]] static std::array<float, 3> readVector(content::BinaryReader& reader);
    [[nodiscard]] static bool readFlag(content::BinaryReader& reader);
    [[nodiscard]] static std::size_t readCount(content::BinaryReader& reader, std::size_t itemSize);
    [[nodiscard]] static BuildSettings readSettings(content::BinaryReader& reader);
    [[nodiscard]] static std::vector<std::string> readAreas(content::BinaryReader& reader);
    [[nodiscard]] static MeshLink readLink(content::BinaryReader& reader, std::size_t areaCount);
    [[nodiscard]] static TileMesh readMesh(content::BinaryReader& reader, const Contents& contents, const TileGrid& grid);
    [[nodiscard]] static TileLayer readLayer(content::BinaryReader& reader, const Contents& contents, const TileGrid& grid);

    // These check what Detour trusts: the vertices and neighbors of every polygon and the vertices of every detail triangle, and the areas and the connections of every voxel of a layer, which must lead to a voxel of the layer.
    static void checkPolygons(content::BinaryReader& reader, const TileMesh& mesh, std::size_t areaCount, int tileCells);
    static void checkDetail(content::BinaryReader& reader, const TileMesh& mesh);
    static void checkVoxels(content::BinaryReader& reader, const TileLayer& layer, std::size_t areaCount);
    static void checkTile(content::BinaryReader& reader, const TileGrid& grid, float cellSize, int x, int z, const std::array<float, 3>& min, const std::array<float, 3>& max);
};

} // namespace haylen::navigation3d
