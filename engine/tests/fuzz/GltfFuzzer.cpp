#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/3d/animation/Clip3D.hpp"
#include "haylen/3d/animation/MorphTargets3D.hpp"
#include "haylen/3d/animation/Skeleton3D.hpp"
#include "haylen/3d/animation/Skin3D.hpp"
#include "haylen/3d/animation/SkinWeights3D.hpp"
#include "haylen/3d/animation/Skinning3D.hpp"
#include "haylen/3d/gltf/AccessorReader.hpp"
#include "haylen/3d/gltf/Reader.hpp"
#include "haylen/graphics/Image.hpp"

namespace haylen::gltf {

// Feeds the glTF reader the inputs libFuzzer generates. An input that starts like a GLB file or a JSON object is a whole file, which covers the container, the JSON and the rules of the document. Any other input gives its first byte to pick a document whose shape is fixed and its other bytes to the data of that document: the binary chunk of a GLB file with accessors of every use, so the rules of the data and the reading of accessors run on hostile bytes, or the text of the `data:` URIs of a buffer and an image. A document that reads has every accessor read, every image decoded within 64 pixels every animation made into a clip that samples at times across it and past both ends, every skin made into a skeleton and a skin whose palette bends the vertices of each primitive it bends with 4 and 8 influences, and the morph targets of every primitive blended at half their weights. Only the failures of reading a file, of decoding an image, of sampling a rotation that a spline passes through zero and of the dual quaternion of a joint whose inverse bind matrix scales an axis to zero may leave, so any other exception, such as a clip or a skin that a valid document does not make, is a bug that the fuzzer reports.
class GltfFuzzer final {
  public:
    static void run(std::span<const std::uint8_t> input) {
        if (input.empty()) {
            return;
        }

        const bool whole = input.size() >= 4 && (std::string_view(reinterpret_cast<const char*>(input.data()), 4) == "glTF" || input[0] == '{');
        if (whole) {
            read(std::vector<std::uint8_t>(input.begin(), input.end()));
            return;
        }

        const std::span<const std::uint8_t> data = input.subspan(1);
        if (input[0] % 2 == 0) {
            read(binaryDocument(data));
            return;
        }

        read(dataUriDocument(data));
    }

  private:
    // Limits that keep each input fast, which the limits of the reader enforce like any other.
    static constexpr haylen::gltf::Limits kLimits{.maxJsonBytes = 1U << 16U, .maxBufferBytes = 1U << 20U, .maxDataUriBytes = 1U << 16U, .maxImageBytes = 1U << 16U, .maxObjects = 4096};

    static void read(const std::vector<std::uint8_t>& bytes) {
        try {
            const haylen::gltf::Document document = haylen::gltf::Reader::read(bytes, "fuzz.glb", kLimits);
            for (std::size_t accessor = 0; accessor < document.accessors.size(); ++accessor) {
                (void)haylen::gltf::AccessorReader::readFloats(document, accessor);
            }

            for (std::size_t image = 0; image < document.images.size(); ++image) {
                decode(document, image);
            }

            for (std::size_t animation = 0; animation < document.animations.size(); ++animation) {
                sample(haylen::animation3d::Clip3D::fromGltf(document, animation));
            }

            for (const haylen::gltf::Node& node : document.nodes) {
                if (node.mesh && node.skin) {
                    skin(document, *node.mesh, *node.skin);
                }
            }

            for (std::size_t mesh = 0; mesh < document.meshes.size(); ++mesh) {
                morph(document, mesh);
            }
        } catch (const haylen::gltf::ReadError&) {}
    }

    static void skin(const haylen::gltf::Document& document, std::size_t mesh, std::size_t skin) {
        auto skeleton = std::make_shared<const haylen::animation3d::Skeleton3D>(haylen::animation3d::Skeleton3D::fromGltf(document, skin));
        const haylen::animation3d::Skin3D joints = haylen::animation3d::Skin3D::fromGltf(document, skin, skeleton);
        const haylen::animation3d::Pose3D pose(skeleton);
        std::vector<haylen::math::Mat4> palette(joints.getJointCount());
        std::vector<haylen::math::DualQuat> dualQuats(joints.getJointCount());
        joints.computePalette(pose, palette);
        try {
            joints.computeDualQuatPalette(pose, dualQuats);
        } catch (const std::domain_error&) {}

        for (std::size_t primitive = 0; primitive < document.meshes[mesh].primitives.size(); ++primitive) {
            const std::optional<std::size_t> accessor = document.meshes[mesh].primitives[primitive].findAttribute("POSITION");
            if (!accessor) {
                continue;
            }

            const std::vector<float> floats = haylen::gltf::AccessorReader::readFloats(document, *accessor);
            std::vector<haylen::math::Vec3> positions(floats.size() / 3);
            for (std::size_t vertex = 0; vertex < positions.size(); ++vertex) {
                positions[vertex] = {floats[vertex * 3], floats[vertex * 3 + 1], floats[vertex * 3 + 2]};
            }

            std::vector<haylen::math::Vec3> skinned(positions.size());
            for (const std::size_t influences : {std::size_t{4}, std::size_t{8}}) {
                const haylen::animation3d::SkinWeights3D weights = haylen::animation3d::SkinWeights3D::fromGltf(document, mesh, primitive, joints.getJointCount(), influences);
                haylen::animation3d::Skinning3D::applyLinear(weights, palette, {.positions = positions, .skinnedPositions = skinned});
            }
        }
    }

    static void morph(const haylen::gltf::Document& document, std::size_t mesh) {
        for (std::size_t primitive = 0; primitive < document.meshes[mesh].primitives.size(); ++primitive) {
            if (document.meshes[mesh].primitives[primitive].targets.empty()) {
                continue;
            }

            const haylen::animation3d::MorphTargets3D targets = haylen::animation3d::MorphTargets3D::fromGltf(document, mesh, primitive);
            const std::vector<haylen::math::Vec3> base(targets.getVertexCount());
            std::vector<haylen::math::Vec3> morphed(targets.getVertexCount());
            const std::vector<float> weights(targets.getTargetCount(), 0.5F);
            targets.apply(weights, {.positions = base, .morphedPositions = morphed});
        }
    }

    static void sample(const haylen::animation3d::Clip3D& clip) {
        std::vector<float> values(clip.getValueCount());
        std::vector<std::size_t> cursors(clip.getTracks().size());
        for (int step = -1; step <= 9; ++step) {
            try {
                clip.sample(clip.getDuration() * static_cast<float>(step) / 8.0F, values, cursors);
            } catch (const std::domain_error&) {}
        }
    }

    static void decode(const haylen::gltf::Document& document, std::size_t image) {
        try {
            (void)document.decodeImage(image, 64);
        } catch (const std::runtime_error&) {}
    }

    // A skinned, animated and morphed triangle whose accessors of every use read from the fuzzed binary chunk, padded with zeros to the bytes the views take.
    [[nodiscard]] static std::vector<std::uint8_t> binaryDocument(std::span<const std::uint8_t> data) {
        constexpr std::size_t kLength = 256;
        const std::string json = R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":256}],)"
                                 R"("bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":36},{"buffer":0,"byteOffset":72,"byteLength":8},)"
                                 R"({"buffer":0,"byteOffset":80,"byteLength":12},{"buffer":0,"byteOffset":92,"byteLength":48},{"buffer":0,"byteOffset":140,"byteLength":64},)"
                                 R"({"buffer":0,"byteOffset":204,"byteLength":8},{"buffer":0,"byteOffset":212,"byteLength":32},{"buffer":0,"byteOffset":244,"byteLength":12}],)"
                                 R"("accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},)"
                                 R"({"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"},)"
                                 R"({"bufferView":3,"componentType":5121,"count":3,"type":"VEC4"},{"bufferView":4,"componentType":5126,"count":3,"type":"VEC4"},)"
                                 R"({"bufferView":5,"componentType":5126,"count":1,"type":"MAT4"},{"bufferView":6,"componentType":5126,"count":2,"type":"SCALAR","min":[0],"max":[1]},)"
                                 R"({"bufferView":7,"componentType":5126,"count":2,"type":"VEC4"},)"
                                 R"({"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[0,0,0],"sparse":{"count":1,"indices":{"bufferView":8,"componentType":5121},"values":{"bufferView":8,"byteOffset":0}}}],)"
                                 R"("meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"JOINTS_0":3,"WEIGHTS_0":4},"indices":2,"targets":[{"POSITION":8}]}]}],)"
                                 R"("skins":[{"joints":[1],"inverseBindMatrices":5}],)"
                                 R"("nodes":[{"mesh":0,"skin":0,"children":[1]},{}],"scenes":[{"nodes":[0]}],)"
                                 R"("animations":[{"samplers":[{"input":6,"output":7}],"channels":[{"sampler":0,"target":{"node":1,"path":"rotation"}}]}]})";
        std::vector<std::uint8_t> binary(data.begin(), data.end());
        binary.resize(kLength, 0);
        return glb(json, binary);
    }

    // A text document whose buffer and image are `data:` URIs of the fuzzed text, the first half for the buffer and the second for the image.
    [[nodiscard]] static std::vector<std::uint8_t> dataUriDocument(std::span<const std::uint8_t> data) {
        std::string text;
        for (const std::uint8_t byte : data) {
            // The characters that would end the string of the JSON become a character of Base64, so the text stays inside its URI.
            text.push_back(byte == '"' || byte == '\\' || byte < 0x20 ? 'A' : static_cast<char>(byte));
        }

        const std::string buffer = text.substr(0, text.size() / 2);
        const std::string image = text.substr(text.size() / 2);
        const std::string json = R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":12,"uri":"data:application/octet-stream;base64,)" + buffer +
                                 R"("}],)"
                                 R"("bufferViews":[{"buffer":0,"byteLength":12}],"accessors":[{"bufferView":0,"componentType":5126,"count":1,"type":"VEC3"}],)"
                                 R"("images":[{"uri":"data:image/png;base64,)" +
                                 image + R"("}],"textures":[{"source":0}]})";
        return std::vector<std::uint8_t>(json.begin(), json.end());
    }

    static void appendWord(std::vector<std::uint8_t>& file, std::size_t value) {
        for (unsigned shift = 0; shift < 32; shift += 8) {
            file.push_back(static_cast<std::uint8_t>(value >> shift));
        }
    }

    [[nodiscard]] static std::vector<std::uint8_t> glb(const std::string& json, const std::vector<std::uint8_t>& binary) {
        std::string padded = json;
        while (padded.size() % 4 != 0) {
            padded.push_back(' ');
        }

        std::vector<std::uint8_t> file;
        appendWord(file, 0x46546C67U);
        appendWord(file, 2);
        appendWord(file, 12 + 8 + padded.size() + 8 + binary.size());
        appendWord(file, padded.size());
        appendWord(file, 0x4E4F534AU);
        file.insert(file.end(), padded.begin(), padded.end());
        appendWord(file, binary.size());
        appendWord(file, 0x004E4942U);
        file.insert(file.end(), binary.begin(), binary.end());
        return file;
    }
};

} // namespace haylen::gltf

// The entry point libFuzzer calls with each input it generates.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    haylen::gltf::GltfFuzzer::run(std::span(data, size));
    return 0;
}
