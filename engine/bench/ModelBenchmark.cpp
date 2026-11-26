#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "haylen/3d/gltf/AccessorReader.hpp"
#include "haylen/3d/gltf/Reader.hpp"
#include "haylen/core/Json.hpp"

namespace haylen::bench {

// Times the reading of a large glTF file on the CPU: a skinned grid of a million vertices with normals, tangents, texture coordinates, joints and weights and two million triangles in a GLB file in memory, through each step of the reader, with the pass of the rules of the data on its own, and the reading of its positions.
class ModelBenchmark final {
  public:
    static int run() {
        const std::vector<std::uint8_t> file = buildGrid();
        std::printf("%-52s %12s %8s\n", "Step", "Average ms", "Runs");
        std::printf("%-52s %12.1f %8s\n", "File size in MiB", static_cast<double>(file.size()) / (1024.0 * 1024.0), "-");

        measure("Open: container, JSON, shape and links", 10, [&file] { (void)gltf::Reader::open(file, "grid.glb"); });
        // clang-format off
        measure("Open and load the buffers", 10, [&file] {
            gltf::Reader reader = gltf::Reader::open(file, "grid.glb");
            reader.loadResources();
        });
        // clang-format on

        // The rules of the data run on a reader that already loaded, so their time stands alone.
        double rules = 0.0;
        constexpr int kRuns = 10;
        gltf::Document document;
        for (int run = 0; run < kRuns; ++run) {
            gltf::Reader reader = gltf::Reader::open(file, "grid.glb");
            reader.loadResources();
            const auto start = std::chrono::steady_clock::now();
            document = reader.finish();
            rules += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        }

        std::printf("%-52s %12.2f %8d\n", "Rules of the data, one pass over 1M vertices", rules / kRuns, kRuns);
        measure("Read the positions as floats", 10, [&document] { (void)gltf::AccessorReader::readFloats(document, 0); });
        return 0;
    }

  private:
    static constexpr std::uint32_t kSide = 1000;

    static void measure(const char* name, int runs, const std::function<void()>& work) {
        work();
        const auto start = std::chrono::steady_clock::now();
        for (int run = 0; run < runs; ++run) {
            work();
        }

        const double total = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        std::printf("%-52s %12.2f %8d\n", name, total / runs, runs);
    }

    template <typename T> static std::uint32_t append(std::vector<std::uint8_t>& binary, const std::vector<T>& values) {
        while (binary.size() % 4 != 0) {
            binary.push_back(0);
        }

        const auto offset = static_cast<std::uint32_t>(binary.size());
        binary.resize(binary.size() + values.size() * sizeof(T));
        std::memcpy(binary.data() + offset, values.data(), values.size() * sizeof(T));
        return offset;
    }

    [[nodiscard]] static std::vector<std::uint8_t> buildGrid() {
        constexpr std::uint32_t kVertices = kSide * kSide;
        std::vector<float> positions;
        std::vector<float> normals;
        std::vector<float> tangents;
        std::vector<float> uvs;
        std::vector<std::uint8_t> joints;
        std::vector<float> weights;
        std::vector<std::uint32_t> indices;
        for (std::uint32_t y = 0; y < kSide; ++y) {
            for (std::uint32_t x = 0; x < kSide; ++x) {
                const float u = static_cast<float>(x) / static_cast<float>(kSide - 1);
                const float v = static_cast<float>(y) / static_cast<float>(kSide - 1);
                positions.insert(positions.end(), {u, 0.0F, v});
                normals.insert(normals.end(), {0.0F, 1.0F, 0.0F});
                tangents.insert(tangents.end(), {1.0F, 0.0F, 0.0F, 1.0F});
                uvs.insert(uvs.end(), {u, v});
                joints.insert(joints.end(), {0, 1, 0, 0});
                weights.insert(weights.end(), {0.75F, 0.25F, 0.0F, 0.0F});
                if (x + 1 < kSide && y + 1 < kSide) {
                    const std::uint32_t corner = y * kSide + x;
                    indices.insert(indices.end(), {corner, corner + kSide, corner + 1, corner + 1, corner + kSide, corner + kSide + 1});
                }
            }
        }

        std::vector<std::uint8_t> binary;
        const std::uint32_t positionAt = append(binary, positions);
        const std::uint32_t normalAt = append(binary, normals);
        const std::uint32_t tangentAt = append(binary, tangents);
        const std::uint32_t uvAt = append(binary, uvs);
        const std::uint32_t jointAt = append(binary, joints);
        const std::uint32_t weightAt = append(binary, weights);
        const std::uint32_t indexAt = append(binary, indices);
        const std::vector<float> matrices{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        const std::uint32_t matrixAt = append(binary, matrices);

        // clang-format off
        const auto view = [](std::uint32_t offset, std::size_t length, int target) {
            core::Json entry = {{"buffer", 0}, {"byteOffset", offset}, {"byteLength", length}};
            if (target != 0) {
                entry["target"] = target;
            }

            return entry;
        };
        // clang-format on
        core::Json document = {
            {"asset", {{"version", "2.0"}}},
            {"buffers", core::Json::array({{{"byteLength", binary.size()}}})},
            {"bufferViews", core::Json::array({view(positionAt, positions.size() * 4, 34962), view(normalAt, normals.size() * 4, 34962), view(tangentAt, tangents.size() * 4, 34962), view(uvAt, uvs.size() * 4, 34962), view(jointAt, joints.size(), 34962), view(weightAt, weights.size() * 4, 34962), view(indexAt, indices.size() * 4, 34963), view(matrixAt, matrices.size() * 4, 0)})},
            {"accessors", core::Json::array({
                {{"bufferView", 0}, {"componentType", 5126}, {"count", kVertices}, {"type", "VEC3"}, {"min", {0, 0, 0}}, {"max", {1, 0, 1}}},
                {{"bufferView", 1}, {"componentType", 5126}, {"count", kVertices}, {"type", "VEC3"}},
                {{"bufferView", 2}, {"componentType", 5126}, {"count", kVertices}, {"type", "VEC4"}},
                {{"bufferView", 3}, {"componentType", 5126}, {"count", kVertices}, {"type", "VEC2"}},
                {{"bufferView", 4}, {"componentType", 5121}, {"count", kVertices}, {"type", "VEC4"}},
                {{"bufferView", 5}, {"componentType", 5126}, {"count", kVertices}, {"type", "VEC4"}},
                {{"bufferView", 6}, {"componentType", 5125}, {"count", indices.size()}, {"type", "SCALAR"}},
                {{"bufferView", 7}, {"componentType", 5126}, {"count", 2}, {"type", "MAT4"}},
            })},
            {"meshes", core::Json::array({{{"primitives", core::Json::array({{{"attributes", {{"POSITION", 0}, {"NORMAL", 1}, {"TANGENT", 2}, {"TEXCOORD_0", 3}, {"JOINTS_0", 4}, {"WEIGHTS_0", 5}}}, {"indices", 6}}})}}})},
            {"skins", core::Json::array({{{"joints", {1, 2}}, {"inverseBindMatrices", 7}}})},
            {"nodes", core::Json::array({{{"mesh", 0}, {"skin", 0}, {"children", {1}}}, {{"children", {2}}}, core::Json::object()})},
            {"scenes", core::Json::array({{{"nodes", {0}}}})},
        };

        std::string json = document.dump();
        while (json.size() % 4 != 0) {
            json.push_back(' ');
        }

        while (binary.size() % 4 != 0) {
            binary.push_back(0);
        }

        std::vector<std::uint8_t> file;
        for (const std::uint32_t word : {0x46546C67U, 2U, static_cast<std::uint32_t>(28 + json.size() + binary.size()), static_cast<std::uint32_t>(json.size()), 0x4E4F534AU}) {
            appendWord(file, word);
        }

        file.insert(file.end(), json.begin(), json.end());
        appendWord(file, static_cast<std::uint32_t>(binary.size()));
        appendWord(file, 0x004E4942U);
        file.insert(file.end(), binary.begin(), binary.end());
        return file;
    }

    static void appendWord(std::vector<std::uint8_t>& file, std::uint32_t value) {
        for (unsigned shift = 0; shift < 32; shift += 8) {
            file.push_back(static_cast<std::uint8_t>(value >> shift));
        }
    }
};

} // namespace haylen::bench

int main() {
    return haylen::bench::ModelBenchmark::run();
}
