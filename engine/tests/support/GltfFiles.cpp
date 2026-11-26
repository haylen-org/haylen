#include "support/GltfFiles.hpp"

#include <array>
#include <cstring>
#include <string>

namespace haylen::test {

std::filesystem::path GltfFiles::getFolder() {
    return HAYLEN_TEST_GLTF;
}

void GltfFiles::appendWord(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        bytes.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

void GltfFiles::pad(std::vector<std::uint8_t>& bytes, std::uint8_t fill) {
    while (bytes.size() % 4 != 0) {
        bytes.push_back(fill);
    }
}

std::uint32_t GltfFiles::append(std::vector<std::uint8_t>& binary, std::span<const float> values) {
    pad(binary, 0);
    const auto offset = static_cast<std::uint32_t>(binary.size());
    binary.resize(binary.size() + values.size_bytes());
    std::memcpy(binary.data() + offset, values.data(), values.size_bytes());
    return offset;
}

std::uint32_t GltfFiles::append(std::vector<std::uint8_t>& binary, std::span<const std::uint16_t> values) {
    pad(binary, 0);
    const auto offset = static_cast<std::uint32_t>(binary.size());
    binary.resize(binary.size() + values.size_bytes());
    std::memcpy(binary.data() + offset, values.data(), values.size_bytes());
    return offset;
}

GltfFiles::File GltfFiles::triangle() {
    File file;
    constexpr std::array<float, 9> kPositions{0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F};
    constexpr std::array<float, 9> kNormals{0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F};
    constexpr std::array<std::uint16_t, 3> kIndices{0, 1, 2};
    const std::uint32_t positions = append(file.binary, kPositions);
    const std::uint32_t normals = append(file.binary, kNormals);
    const std::uint32_t indices = append(file.binary, kIndices);
    file.document = core::Json::parse(R"({
        "asset": {"version": "2.0"},
        "buffers": [{"byteLength": 0}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": 36, "target": 34962},
            {"buffer": 0, "byteOffset": 36, "byteLength": 36, "target": 34962},
            {"buffer": 0, "byteOffset": 72, "byteLength": 6, "target": 34963}
        ],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3", "min": [0, 0, 0], "max": [1, 1, 0]},
            {"bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC3"},
            {"bufferView": 2, "componentType": 5123, "count": 3, "type": "SCALAR"}
        ],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 2}]}],
        "nodes": [{"mesh": 0}],
        "scenes": [{"nodes": [0]}],
        "scene": 0
    })");
    file.document["buffers"][0]["byteLength"] = file.binary.size();
    file.document["bufferViews"][0]["byteOffset"] = positions;
    file.document["bufferViews"][1]["byteOffset"] = normals;
    file.document["bufferViews"][2]["byteOffset"] = indices;
    return file;
}

std::vector<std::uint8_t> GltfFiles::glb(const core::Json& document, std::span<const std::uint8_t> binary) {
    std::string json = document.dump();
    while (json.size() % 4 != 0) {
        json.push_back(' ');
    }

    std::vector<std::uint8_t> chunks;
    appendWord(chunks, static_cast<std::uint32_t>(json.size()));
    appendWord(chunks, 0x4E4F534AU);
    chunks.insert(chunks.end(), json.begin(), json.end());
    if (!binary.empty()) {
        std::vector<std::uint8_t> padded(binary.begin(), binary.end());
        pad(padded, 0);
        appendWord(chunks, static_cast<std::uint32_t>(padded.size()));
        appendWord(chunks, 0x004E4942U);
        chunks.insert(chunks.end(), padded.begin(), padded.end());
    }

    std::vector<std::uint8_t> file;
    appendWord(file, 0x46546C67U);
    appendWord(file, 2);
    appendWord(file, static_cast<std::uint32_t>(12 + chunks.size()));
    file.insert(file.end(), chunks.begin(), chunks.end());
    return file;
}

std::vector<std::uint8_t> GltfFiles::glb(const File& file) {
    return glb(file.document, file.binary);
}

} // namespace haylen::test
