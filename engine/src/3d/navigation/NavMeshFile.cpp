#include "3d/navigation/NavMeshFile.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <set>
#include <stdexcept>
#include <tuple>

#include "3d/navigation/LayerCodec.hpp"
#include "3d/navigation/PolygonValidator.hpp"
#include "content/format/BinaryReader.hpp"
#include "content/format/BinaryWriter.hpp"
#include "haylen/3d/navigation/SourceGeometry.hpp"

namespace haylen::navigation3d {

void NavMeshFile::writeFloat(content::BinaryWriter& writer, float value) {
    writer.write(std::bit_cast<std::uint32_t>(value));
}

void NavMeshFile::writeVector(content::BinaryWriter& writer, const std::array<float, 3>& value) {
    for (const float component : value) {
        writeFloat(writer, component);
    }
}

void NavMeshFile::writeSettings(content::BinaryWriter& writer, const BuildSettings& settings) {
    for (const float value : {settings.cellSize, settings.cellHeight, settings.agentHeight, settings.agentRadius, settings.agentMaxClimb, settings.agentMaxSlope, settings.regionMinArea, settings.regionMergeArea, settings.edgeMaxLength, settings.edgeMaxError, settings.detailSampleDistance, settings.detailSampleMaxError, settings.tileSize}) {
        writeFloat(writer, value);
    }

    writer.write(static_cast<std::uint8_t>(settings.polygonMaxVertices));
    writer.write(static_cast<std::uint8_t>(settings.partition));
    writer.write(static_cast<std::uint8_t>(settings.filterLowObstacles));
    writer.write(static_cast<std::uint8_t>(settings.filterLedges));
    writer.write(static_cast<std::uint8_t>(settings.filterLowCeilings));
    writer.write(static_cast<std::uint8_t>(settings.obstacles));
    writer.write(static_cast<std::uint32_t>(settings.maxObstacles));
    writer.write(static_cast<std::uint8_t>(settings.bounds.has_value()));
    if (settings.bounds) {
        writeVector(writer, {settings.bounds->min.x, settings.bounds->min.y, settings.bounds->min.z});
        writeVector(writer, {settings.bounds->max.x, settings.bounds->max.y, settings.bounds->max.z});
    }
}

void NavMeshFile::writeMesh(content::BinaryWriter& writer, const TileMesh& mesh) {
    writer.write(std::bit_cast<std::uint32_t>(mesh.x)).write(std::bit_cast<std::uint32_t>(mesh.z));
    writeVector(writer, mesh.min);
    writeVector(writer, mesh.max);
    writer.write(static_cast<std::uint32_t>(mesh.getVertexCount()));
    writer.write(static_cast<std::uint32_t>(mesh.getPolygonCount()));
    writer.write(static_cast<std::uint32_t>(mesh.detailVertices.size() / 3));
    writer.write(static_cast<std::uint32_t>(mesh.detailTriangles.size() / 4));
    for (const std::uint16_t value : mesh.vertices) {
        writer.write(value);
    }
    for (const std::uint16_t value : mesh.polygons) {
        writer.write(value);
    }
    writer.writeBytes(mesh.areas);
    for (const std::uint32_t value : mesh.detailMeshes) {
        writer.write(value);
    }
    for (const float value : mesh.detailVertices) {
        writeFloat(writer, value);
    }
    writer.writeBytes(mesh.detailTriangles);
}

void NavMeshFile::writeLayer(content::BinaryWriter& writer, const TileLayer& layer) {
    writer.write(std::bit_cast<std::uint32_t>(layer.x)).write(std::bit_cast<std::uint32_t>(layer.z)).write(static_cast<std::uint8_t>(layer.layer));
    writeVector(writer, layer.min);
    writeVector(writer, layer.max);
    writer.write(layer.minHeight).write(layer.maxHeight);
    writer.write(layer.width).write(layer.height).write(layer.minX).write(layer.maxX).write(layer.minZ).write(layer.maxZ);
    writer.write(static_cast<std::uint32_t>(layer.compressed.size()));
    writer.writeBytes(layer.compressed);
}

std::vector<std::uint8_t> NavMeshFile::encode(const Contents& contents) {
    content::BinaryWriter writer;
    writer.write(kMagic).write(kVersion);
    writeSettings(writer, contents.settings);
    writeVector(writer, {contents.bounds.min.x, contents.bounds.min.y, contents.bounds.min.z});
    writeVector(writer, {contents.bounds.max.x, contents.bounds.max.y, contents.bounds.max.z});

    writer.write(static_cast<std::uint32_t>(contents.areas.size()));
    for (const std::string& area : contents.areas) {
        writer.writeString(area);
    }

    writer.write(static_cast<std::uint32_t>(contents.links.size()));
    for (const MeshLink& link : contents.links) {
        writer.write(link.id);
        writeVector(writer, {link.link.start.x, link.link.start.y, link.link.start.z});
        writeVector(writer, {link.link.end.x, link.link.end.y, link.link.end.z});
        writeFloat(writer, link.link.radius);
        writer.write(static_cast<std::uint8_t>(link.link.bidirectional)).write(static_cast<std::uint8_t>(link.link.kind)).write(link.area);
    }

    writer.write(static_cast<std::uint32_t>(contents.meshes.size()));
    for (const TileMesh& mesh : contents.meshes) {
        writeMesh(writer, mesh);
    }

    writer.write(static_cast<std::uint32_t>(contents.layers.size()));
    for (const TileLayer& layer : contents.layers) {
        writeLayer(writer, layer);
    }

    return writer.take();
}

float NavMeshFile::readFloat(content::BinaryReader& reader) {
    return std::bit_cast<float>(reader.read<std::uint32_t>());
}

float NavMeshFile::readFinite(content::BinaryReader& reader) {
    const float value = readFloat(reader);
    if (!std::isfinite(value)) {
        reader.fail();
    }

    return value;
}

float NavMeshFile::readCoordinate(content::BinaryReader& reader) {
    const float value = readFinite(reader);
    if (std::fabs(value) > SourceGeometry::kMaxCoordinate) {
        reader.fail();
    }

    return value;
}

std::array<float, 3> NavMeshFile::readVector(content::BinaryReader& reader) {
    return {readCoordinate(reader), readCoordinate(reader), readCoordinate(reader)};
}

bool NavMeshFile::readFlag(content::BinaryReader& reader) {
    const auto value = reader.read<std::uint8_t>();
    if (value > 1) {
        reader.fail();
    }

    return value == 1;
}

std::size_t NavMeshFile::readCount(content::BinaryReader& reader, std::size_t itemSize) {
    const std::size_t count = reader.read<std::uint32_t>();
    if (count > reader.getRemaining() / itemSize) {
        reader.fail();
    }

    return count;
}

BuildSettings NavMeshFile::readSettings(content::BinaryReader& reader) {
    BuildSettings settings;
    for (float* value : {&settings.cellSize, &settings.cellHeight, &settings.agentHeight, &settings.agentRadius, &settings.agentMaxClimb, &settings.agentMaxSlope, &settings.regionMinArea, &settings.regionMergeArea, &settings.edgeMaxLength, &settings.edgeMaxError, &settings.detailSampleDistance, &settings.detailSampleMaxError, &settings.tileSize}) {
        *value = readFinite(reader);
    }

    settings.polygonMaxVertices = reader.read<std::uint8_t>();
    const auto partition = reader.read<std::uint8_t>();
    if (partition > static_cast<std::uint8_t>(BuildSettings::Partition::Layers)) {
        reader.fail();
    }

    settings.partition = static_cast<BuildSettings::Partition>(partition);
    settings.filterLowObstacles = readFlag(reader);
    settings.filterLedges = readFlag(reader);
    settings.filterLowCeilings = readFlag(reader);
    settings.obstacles = readFlag(reader);
    settings.maxObstacles = reader.read<std::uint32_t>();
    if (readFlag(reader)) {
        const std::array<float, 3> min = readVector(reader);
        const std::array<float, 3> max = readVector(reader);
        settings.bounds = math::Box{{min[0], min[1], min[2]}, {max[0], max[1], max[2]}};
    }

    try {
        settings.validate();
    } catch (const std::invalid_argument&) {
        reader.fail();
    }

    return settings;
}

std::vector<std::string> NavMeshFile::readAreas(content::BinaryReader& reader) {
    const std::size_t count = readCount(reader, 2);
    if (count == 0 || count > SourceGeometry::kMaxAreas) {
        reader.fail();
    }

    std::vector<std::string> areas;
    areas.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const std::string_view name = reader.readString(kMaxAreaName);
        const bool repeated = std::ranges::find(areas, name) != areas.end();
        if (name.empty() || name == SourceGeometry::kNone || repeated || (index == 0) != (name == SourceGeometry::kGround)) {
            reader.fail();
        }

        areas.emplace_back(name);
    }

    return areas;
}

MeshLink NavMeshFile::readLink(content::BinaryReader& reader, std::size_t areaCount) {
    MeshLink link;
    link.id = reader.read<std::uint32_t>();
    const std::array<float, 3> start = readVector(reader);
    const std::array<float, 3> end = readVector(reader);
    link.link.start = {start[0], start[1], start[2]};
    link.link.end = {end[0], end[1], end[2]};
    link.link.radius = readCoordinate(reader);
    link.link.bidirectional = readFlag(reader);
    const auto kind = reader.read<std::uint8_t>();
    link.area = reader.read<std::uint8_t>();
    if (link.id == 0 || !(link.link.radius > 0.0F) || kind > static_cast<std::uint8_t>(Link::Kind::Teleport) || link.area > areaCount) {
        reader.fail();
    }

    link.link.kind = static_cast<Link::Kind>(kind);
    return link;
}

void NavMeshFile::checkTile(content::BinaryReader& reader, const TileGrid& grid, float cellSize, int x, int z, const std::array<float, 3>& min, const std::array<float, 3>& max) {
    if (x < 0 || z < 0 || x >= grid.columns || z >= grid.rows || min[0] > max[0] || min[1] > max[1] || min[2] > max[2]) {
        reader.fail();
    }

    // A tile covers its own column of the grid on the ground, within half a voxel, so tiles never overlap.
    const math::Box column = grid.getColumnBounds(x, z);
    const float tolerance = cellSize * 0.5F;
    const bool placed = std::fabs(min[0] - column.min.x) <= tolerance && std::fabs(min[2] - column.min.z) <= tolerance && std::fabs(max[0] - column.max.x) <= tolerance && std::fabs(max[2] - column.max.z) <= tolerance;
    if (!placed) {
        reader.fail();
    }
}

void NavMeshFile::checkPolygons(content::BinaryReader& reader, const TileMesh& mesh, std::size_t areaCount, int tileCells) {
    const PolygonValidator::Polygons polygons{.vertices = mesh.vertices, .polygons = mesh.polygons, .areas = mesh.areas, .maxVertices = mesh.maxVertices, .areaCount = areaCount, .width = tileCells, .depth = tileCells};
    if (!PolygonValidator::isValid(polygons)) {
        reader.fail();
    }
}

void NavMeshFile::checkDetail(content::BinaryReader& reader, const TileMesh& mesh) {
    const auto stride = static_cast<std::size_t>(mesh.maxVertices);
    const std::size_t detailVertexCount = mesh.detailVertices.size() / 3;
    const std::size_t detailTriangleCount = mesh.detailTriangles.size() / 4;
    std::size_t extraVertices = 0;
    for (std::size_t polygon = 0; polygon < mesh.getPolygonCount(); ++polygon) {
        const std::uint16_t* corners = &mesh.polygons[polygon * stride * 2];
        const auto used = static_cast<std::size_t>(std::find(corners, corners + stride, std::uint16_t{0xFFFF}) - corners);
        const std::uint64_t firstVertex = mesh.detailMeshes[polygon * 4];
        const std::uint64_t vertices = mesh.detailMeshes[polygon * 4 + 1];
        const std::uint64_t firstTriangle = mesh.detailMeshes[polygon * 4 + 2];
        const std::uint64_t triangles = mesh.detailMeshes[polygon * 4 + 3];
        if (vertices < used || vertices - used > 255 || firstVertex + vertices > detailVertexCount || triangles == 0 || triangles > 255 || firstTriangle + triangles > detailTriangleCount) {
            reader.fail();
        }

        // The closest point of a polygon is searched along the edges that its detail triangles mark as its outline, so every polygon marks some.
        extraVertices += static_cast<std::size_t>(vertices) - used;
        bool outlined = false;
        for (std::uint64_t triangle = firstTriangle; triangle < firstTriangle + triangles; ++triangle) {
            const std::uint8_t* corner = &mesh.detailTriangles[static_cast<std::size_t>(triangle) * 4];
            if (corner[0] >= vertices || corner[1] >= vertices || corner[2] >= vertices) {
                reader.fail();
            }
            outlined = outlined || (corner[3] & kOutlineEdges) != 0;
        }
        if (!outlined) {
            reader.fail();
        }
    }

    // Detour numbers the detail vertices beyond the corners of the polygons with 16 bits.
    if (extraVertices > 0xFFFF) {
        reader.fail();
    }
}

TileMesh NavMeshFile::readMesh(content::BinaryReader& reader, const Contents& contents, const TileGrid& grid) {
    TileMesh mesh;
    mesh.x = std::bit_cast<std::int32_t>(reader.read<std::uint32_t>());
    mesh.z = std::bit_cast<std::int32_t>(reader.read<std::uint32_t>());
    mesh.min = readVector(reader);
    mesh.max = readVector(reader);
    mesh.maxVertices = static_cast<int>(contents.settings.polygonMaxVertices);
    checkTile(reader, grid, contents.settings.cellSize, mesh.x, mesh.z, mesh.min, mesh.max);

    // The counts are checked against the bytes that remain in 64 bits, so no product wraps on a 32-bit target.
    const std::uint64_t vertexCount = reader.read<std::uint32_t>();
    const std::uint64_t polygonCount = reader.read<std::uint32_t>();
    const std::uint64_t detailVertexCount = reader.read<std::uint32_t>();
    const std::uint64_t detailTriangleCount = reader.read<std::uint32_t>();
    const auto stride = static_cast<std::size_t>(mesh.maxVertices) * 2;
    const std::uint64_t size = vertexCount * 6 + polygonCount * (stride * 2 + 1 + 16) + detailVertexCount * 12 + detailTriangleCount * 4;
    if (vertexCount == 0 || vertexCount >= 0xFFFF || polygonCount == 0 || polygonCount >= 0xFFFF || size > reader.getRemaining()) {
        reader.fail();
    }

    mesh.vertices.resize(static_cast<std::size_t>(vertexCount) * 3);
    for (std::uint16_t& value : mesh.vertices) {
        value = reader.read<std::uint16_t>();
    }
    mesh.polygons.resize(static_cast<std::size_t>(polygonCount) * stride);
    for (std::uint16_t& value : mesh.polygons) {
        value = reader.read<std::uint16_t>();
    }
    const std::span<const std::uint8_t> areas = reader.readBytes(static_cast<std::size_t>(polygonCount));
    mesh.areas.assign(areas.begin(), areas.end());
    mesh.detailMeshes.resize(static_cast<std::size_t>(polygonCount) * 4);
    for (std::uint32_t& value : mesh.detailMeshes) {
        value = reader.read<std::uint32_t>();
    }
    mesh.detailVertices.resize(static_cast<std::size_t>(detailVertexCount) * 3);
    for (float& value : mesh.detailVertices) {
        value = readCoordinate(reader);
    }
    const std::span<const std::uint8_t> triangles = reader.readBytes(static_cast<std::size_t>(detailTriangleCount) * 4);
    mesh.detailTriangles.assign(triangles.begin(), triangles.end());

    checkPolygons(reader, mesh, contents.areas.size(), grid.tileCells);
    checkDetail(reader, mesh);
    return mesh;
}

void NavMeshFile::checkVoxels(content::BinaryReader& reader, const TileLayer& layer, std::size_t areaCount) {
    std::vector<std::uint8_t> voxels;
    const std::size_t cells = layer.getCellCount();
    if (!LayerCodec::decompress(layer.compressed, cells * 3, voxels)) {
        reader.fail();
    }

    // Each of the four low bits of a connection leads to the voxel at its side, in the order of Recast: toward smaller x, larger z, larger x and smaller z.
    const int width = layer.width;
    const int height = layer.height;
    for (int z = 0; z < height; ++z) {
        for (int x = 0; x < width; ++x) {
            const auto cell = static_cast<std::size_t>(z) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
            const std::uint8_t area = voxels[cells + cell];
            const std::uint8_t connections = voxels[cells * 2 + cell];
            const bool outside = ((connections & 1U) != 0 && x == 0) || ((connections & 2U) != 0 && z + 1 == height) || ((connections & 4U) != 0 && x + 1 == width) || ((connections & 8U) != 0 && z == 0);
            if (area > areaCount || outside) {
                reader.fail();
            }
        }
    }
}

TileLayer NavMeshFile::readLayer(content::BinaryReader& reader, const Contents& contents, const TileGrid& grid) {
    TileLayer layer;
    layer.x = std::bit_cast<std::int32_t>(reader.read<std::uint32_t>());
    layer.z = std::bit_cast<std::int32_t>(reader.read<std::uint32_t>());
    layer.layer = reader.read<std::uint8_t>();
    layer.min = readVector(reader);
    layer.max = readVector(reader);
    checkTile(reader, grid, contents.settings.cellSize, layer.x, layer.z, layer.min, layer.max);

    layer.minHeight = reader.read<std::uint16_t>();
    layer.maxHeight = reader.read<std::uint16_t>();
    layer.width = reader.read<std::uint8_t>();
    layer.height = reader.read<std::uint8_t>();
    layer.minX = reader.read<std::uint8_t>();
    layer.maxX = reader.read<std::uint8_t>();
    layer.minZ = reader.read<std::uint8_t>();
    layer.maxZ = reader.read<std::uint8_t>();
    if (layer.width == 0 || layer.height == 0 || layer.minX > layer.maxX || layer.maxX >= layer.width || layer.minZ > layer.maxZ || layer.maxZ >= layer.height || layer.minHeight > layer.maxHeight) {
        reader.fail();
    }

    const std::span<const std::uint8_t> compressed = reader.readBytes(readCount(reader, 1));
    layer.compressed.assign(compressed.begin(), compressed.end());
    checkVoxels(reader, layer, contents.areas.size());
    return layer;
}

std::string NavMeshFile::getSubject(std::string_view file) {
    return "The navigation mesh \"" + std::string(file) + "\"";
}

std::string NavMeshFile::describeDamage(std::string_view file) {
    return content::BinaryReader::describeDamage(getSubject(file));
}

NavMeshFile::Contents NavMeshFile::decode(std::span<const std::uint8_t> bytes, std::string_view file) {
    content::BinaryReader reader(bytes, content::Error::Code::UnsupportedFormat, getSubject(file));
    if (reader.read<std::uint32_t>() != kMagic || reader.read<std::uint32_t>() != kVersion) {
        reader.fail();
    }

    Contents contents;
    contents.settings = readSettings(reader);
    const std::array<float, 3> min = readVector(reader);
    const std::array<float, 3> max = readVector(reader);
    contents.bounds = {{min[0], min[1], min[2]}, {max[0], max[1], max[2]}};
    if (contents.bounds.isEmpty()) {
        reader.fail();
    }

    TileGrid grid;
    try {
        grid = TileGrid::make(contents.settings, contents.bounds);
    } catch (const std::invalid_argument&) {
        reader.fail();
    }

    contents.areas = readAreas(reader);
    const std::size_t linkCount = readCount(reader, 32);
    std::set<std::uint32_t> linkIds;
    for (std::size_t index = 0; index < linkCount; ++index) {
        MeshLink link = readLink(reader, contents.areas.size());
        if (!linkIds.insert(link.id).second) {
            reader.fail();
        }
        contents.links.push_back(std::move(link));
    }

    // Every place of a tile holds one tile, and a mesh holds polygons or layers by the kind it was baked as.
    std::set<std::tuple<int, int, int>> places;
    const std::size_t meshCount = readCount(reader, 40);
    for (std::size_t index = 0; index < meshCount; ++index) {
        TileMesh mesh = readMesh(reader, contents, grid);
        if (contents.settings.obstacles || !places.emplace(mesh.x, mesh.z, mesh.layer).second) {
            reader.fail();
        }
        contents.meshes.push_back(std::move(mesh));
    }

    const std::size_t layerCount = readCount(reader, 41);
    for (std::size_t index = 0; index < layerCount; ++index) {
        TileLayer layer = readLayer(reader, contents, grid);
        if (!contents.settings.obstacles || !places.emplace(layer.x, layer.z, layer.layer).second) {
            reader.fail();
        }
        contents.layers.push_back(std::move(layer));
    }

    reader.requireEnd();
    return contents;
}

} // namespace haylen::navigation3d
