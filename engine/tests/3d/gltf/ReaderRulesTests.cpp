#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "haylen/3d/gltf/Reader.hpp"
#include "haylen/io/Package.hpp"
#include "support/GltfFiles.hpp"
#include "support/TestFiles.hpp"

namespace haylen::gltf {

class ReaderRulesTest : public ::testing::Test {
  protected:
    using Problem = ReadError::Problem;
    using Change = std::function<void(test::GltfFiles::File&)>;

    // The problem and the message of the failure of reading bytes, or nothing when they read.
    struct Failure {
        Problem problem = Problem::NotGltf;
        std::string message;
    };

    [[nodiscard]] static std::optional<Failure> failure(const std::vector<std::uint8_t>& bytes, std::string_view name, Limits limits = {}) {
        try {
            (void)Reader::read(bytes, name, limits);
        } catch (const ReadError& error) {
            return Failure{error.getProblem(), error.what()};
        }

        return std::nullopt;
    }

    // The problem of the triangle after a change, which a test expects to break one rule.
    [[nodiscard]] static std::optional<Problem> problemOf(const Change& change) {
        test::GltfFiles::File file = test::GltfFiles::triangle();
        change(file);
        const std::optional<Failure> found = failure(test::GltfFiles::glb(file), "models/triangle.glb");
        return found ? std::optional(found->problem) : std::nullopt;
    }

    [[nodiscard]] static std::string messageOf(const Change& change) {
        test::GltfFiles::File file = test::GltfFiles::triangle();
        change(file);
        const std::optional<Failure> found = failure(test::GltfFiles::glb(file), "models/triangle.glb");
        return found ? found->message : std::string();
    }

    [[nodiscard]] static std::optional<Failure> readPackaged(std::string_view path, Limits limits = {}) {
        static const std::unique_ptr<io::Package> files = io::Package::openDirectory(test::GltfFiles::getFolder());
        try {
            (void)Reader::read(*files, path, limits);
        } catch (const ReadError& error) {
            return Failure{error.getProblem(), error.what()};
        }

        return std::nullopt;
    }

    // Writes a float of the binary chunk at a byte offset.
    static void setFloat(test::GltfFiles::File& file, std::size_t offset, float value) {
        std::memcpy(file.binary.data() + offset, &value, sizeof(value));
    }
};

// The triangle that the rules change reads as it is.
TEST_F(ReaderRulesTest, ReadsTheTriangleThatTheRulesChange) {
    EXPECT_EQ(problemOf([](test::GltfFiles::File&) {}), std::nullopt);
}

// The broken files of the tests each fail with the problem of their one mistake.
TEST_F(ReaderRulesTest, RefusesEveryBrokenFile) {
    EXPECT_EQ(readPackaged("invalid/truncated.glb")->problem, Problem::GlbLength);
    EXPECT_EQ(readPackaged("invalid/wrong-magic.glb")->problem, Problem::NotGltf);
    EXPECT_EQ(readPackaged("invalid/wrong-length.glb")->problem, Problem::GlbLength);
    EXPECT_EQ(readPackaged("invalid/corrupt-json.gltf")->problem, Problem::InvalidJson);
    EXPECT_EQ(readPackaged("invalid/accessor-outside-buffer.gltf")->problem, Problem::AccessorOutOfRange);
    EXPECT_EQ(readPackaged("invalid/duplicate-sparse-index.gltf")->problem, Problem::InvalidSparseIndices);
    EXPECT_EQ(readPackaged("invalid/uri-parent.gltf")->problem, Problem::FileOutsidePackage);
    EXPECT_EQ(readPackaged("invalid/uri-absolute.gltf")->problem, Problem::FileOutsidePackage);
    EXPECT_EQ(readPackaged("invalid/uri-network.gltf")->problem, Problem::UriScheme);
    EXPECT_EQ(readPackaged("invalid/invalid-base64.gltf")->problem, Problem::InvalidUri);
    EXPECT_EQ(readPackaged("invalid/data-uri-too-large.gltf", {.maxDataUriBytes = 64})->problem, Problem::LimitExceeded);
    EXPECT_EQ(readPackaged("invalid/data-uri-too-large.gltf"), std::nullopt);
    EXPECT_EQ(readPackaged("missing.glb")->problem, Problem::MissingFile);
}

// The sentences of the failures name the file, the object in words and the rule, and say what to do.
TEST_F(ReaderRulesTest, NamesTheFileTheObjectAndTheRule) {
    EXPECT_EQ(readPackaged("invalid/uri-parent.gltf")->message, "The glTF file \"invalid/uri-parent.gltf\" names the URI \"../../outside.bin\" of buffer 0, which leaves the content folder of the package. A glTF file reads only files inside its package, which it names by paths relative to itself, such as \"textures/wood.png\".");
    EXPECT_EQ(readPackaged("invalid/accessor-outside-buffer.gltf")->message, "The glTF file \"invalid/accessor-outside-buffer.gltf\" is not valid, since accessor 0 reads past the end of buffer view 1. Export the model again from the tool that made it, so it follows glTF 2.0.");
    EXPECT_EQ(messageOf([](test::GltfFiles::File& file) { file.document["meshes"][0]["primitives"][0]["indices"] = 9; }), "The glTF file \"models/triangle.glb\" is not valid, since the key \"indices\" of primitive 0 of mesh 0 names accessor 9, which the file lacks. Export the model again from the tool that made it, so it follows glTF 2.0.");
    EXPECT_EQ(readPackaged("invalid/corrupt-json.gltf")->message.rfind("The file \"invalid/corrupt-json.gltf\" is not valid JSON. Check line ", 0), 0U);
}

// A GLB file has the magic, the version 2 and a length that matches, then the JSON chunk first and at most one binary chunk second, every chunk a multiple of four bytes.
TEST_F(ReaderRulesTest, RefusesABrokenContainer) {
    const std::vector<std::uint8_t> valid = test::GltfFiles::glb(test::GltfFiles::triangle());
    // clang-format off
    const auto changed = [&valid](std::size_t offset, std::uint32_t value) {
        std::vector<std::uint8_t> bytes = valid;
        std::memcpy(bytes.data() + offset, &value, sizeof(value));
        return failure(bytes, "models/triangle.glb")->problem;
    };
    // clang-format on
    EXPECT_EQ(changed(4, 1), Problem::GlbHeader);
    EXPECT_EQ(changed(8, 16), Problem::GlbLength);
    EXPECT_EQ(changed(12, 6), Problem::GlbChunk);
    EXPECT_EQ(changed(16, 0x004E4942U), Problem::GlbChunk);
    EXPECT_EQ(failure(std::vector<std::uint8_t>(valid.begin(), valid.begin() + 10), "models/triangle.glb")->problem, Problem::GlbHeader);

    // A file may hold bytes past the length its header declares, which belong to no chunk.
    std::vector<std::uint8_t> longer = valid;
    longer.resize(longer.size() + 8, 0xAA);
    EXPECT_EQ(failure(longer, "models/triangle.glb"), std::nullopt);

    // Without a binary chunk, the first buffer has no bytes to take.
    test::GltfFiles::File file = test::GltfFiles::triangle();
    EXPECT_EQ(failure(test::GltfFiles::glb(file.document, {}), "models/triangle.glb")->problem, Problem::GlbMissingBinary);
    file.document["buffers"][0]["byteLength"] = file.binary.size() + 8;
    EXPECT_EQ(failure(test::GltfFiles::glb(file), "models/triangle.glb")->problem, Problem::ResourceTooShort);
}

// The text of a document is a JSON object of a known version, within the bounds of size and nesting.
TEST_F(ReaderRulesTest, RefusesTextThatIsNotADocumentOfGltf2) {
    EXPECT_EQ(failure(test::TestFiles::bytes("PK\x03\x04"), "models/archive.gltf")->problem, Problem::NotGltf);
    EXPECT_EQ(failure(test::TestFiles::bytes("[1, 2]"), "models/list.gltf")->problem, Problem::NotGltf);
    EXPECT_EQ(failure(test::TestFiles::bytes("{\"asset\": {\"version\": \"2.0\"}"), "models/cut.gltf")->problem, Problem::InvalidJson);
    EXPECT_EQ(failure(test::TestFiles::bytes("{\"asset\": {\"version\": \"2.0\"}, \"extras\": " + std::string(200, '[') + std::string(200, ']') + "}"), "models/deep.gltf")->problem, Problem::InvalidJson);
    EXPECT_EQ(failure(test::TestFiles::bytes("{\"asset\": {\"version\": \"2.0\"}}"), "models/big.gltf", {.maxJsonBytes = 8})->problem, Problem::LimitExceeded);
    EXPECT_EQ(failure(test::TestFiles::bytes("{\"asset\": {\"version\": \"2.0\"}}"), "models/empty.gltf"), std::nullopt);
    EXPECT_EQ(failure(test::TestFiles::bytes("{}"), "models/empty.gltf")->problem, Problem::MissingProperty);
    EXPECT_EQ(failure(test::TestFiles::bytes("{\"asset\": {\"version\": \"1.0\"}}"), "models/old.gltf")->problem, Problem::UnsupportedVersion);
    EXPECT_EQ(failure(test::TestFiles::bytes("{\"asset\": {\"version\": \"2.0\", \"minVersion\": \"2.1\"}}"), "models/newer.gltf")->problem, Problem::InvalidValue);
    EXPECT_EQ(failure(test::TestFiles::bytes("{\"asset\": {\"version\": \"2.1\", \"minVersion\": \"2.1\"}}"), "models/newer.gltf")->problem, Problem::UnsupportedVersion);
    EXPECT_EQ(failure(test::TestFiles::bytes("{\"asset\": {\"version\": \"2.1\"}}"), "models/minor.gltf"), std::nullopt);
    EXPECT_EQ(failure(test::TestFiles::bytes("{\"asset\": {\"version\": \"two\"}}"), "models/words.gltf")->problem, Problem::InvalidValue);
}

// An extension that a file requires and the reader lacks fails with its name, AVIF images fail by their format, and an extension that the file only uses must be declared.
TEST_F(ReaderRulesTest, RefusesExtensionsItCannotRead) {
    // clang-format off
    const auto requiring = [](std::string_view extension) {
        return [extension](test::GltfFiles::File& file) {
            file.document["extensionsUsed"] = {extension};
            file.document["extensionsRequired"] = {extension};
        };
    };
    // clang-format on
    EXPECT_EQ(problemOf(requiring("KHR_draco_mesh_compression")), Problem::UnsupportedExtension);
    EXPECT_EQ(messageOf(requiring("KHR_materials_pbrSpecularGlossiness")), "The glTF file \"models/triangle.glb\" requires the extension \"KHR_materials_pbrSpecularGlossiness\", which Haylen does not support. Export the model again without that extension.");
    EXPECT_EQ(problemOf(requiring("EXT_texture_avif")), Problem::UnsupportedImageFormat);
    EXPECT_EQ(problemOf(requiring("EXT_texture_webp")), std::nullopt);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["extensionsRequired"] = {"EXT_texture_webp"}; }), Problem::UndeclaredExtension);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["nodes"][0]["extensions"] = {{"VENDOR_thing", {}}}; }), Problem::UndeclaredExtension);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["extensionsUsed"] = {"VENDOR_thing", "VENDOR_thing"}; }), Problem::DuplicateElement);

    // An extension that the file only uses and the reader does not know stays as its JSON.
    test::GltfFiles::File file = test::GltfFiles::triangle();
    file.document["extensionsUsed"] = {"VENDOR_thing"};
    file.document["nodes"][0]["extensions"] = {{"VENDOR_thing", {{"speed", 3}}}};
    const Document document = Reader::read(test::GltfFiles::glb(file), "models/triangle.glb");
    EXPECT_EQ(document.nodes[0].extensions.at("VENDOR_thing").at("speed"), 3);
}

// Every value has the type, the range and the value of an enum that the specification gives it, whole numbers may carry a fraction of zero, and an index names an object the file has.
TEST_F(ReaderRulesTest, RefusesValuesOfTheWrongTypeOrRange) {
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["accessors"][0]["count"] = "3"; }), Problem::WrongType);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["accessors"][0]["count"] = 2.5; }), Problem::WrongType);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["accessors"][2]["count"] = 3.0; }), std::nullopt);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["accessors"][0]["count"] = 0; }), Problem::InvalidValue);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["accessors"][0]["count"] = -3; }), Problem::InvalidValue);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["accessors"][0]["componentType"] = 5124; }), Problem::InvalidValue);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["accessors"][0]["type"] = "VEC5"; }), Problem::InvalidValue);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["accessors"][2]["normalized"] = true; }), Problem::AccessorFormat);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["accessors"][0]["normalized"] = true; }), Problem::InvalidValue);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["accessors"][0].erase("type"); }), Problem::MissingProperty);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["nodes"][0]["mesh"] = 1; }), Problem::InvalidIndex);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["scene"] = 3; }), Problem::InvalidIndex);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["nodes"] = core::Json::array(); }), Problem::EmptyElement);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["nodes"] = {7}; }), Problem::WrongType);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["nodes"][0]["translation"] = {1, 2}; }), Problem::WrongType);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["nodes"][0]["translation"] = {1, 2, 1e39}; }), Problem::InvalidValue);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["materials"] = {{{"alphaMode", "CLEAR"}}}; }), Problem::InvalidValue);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["materials"] = {{{"pbrMetallicRoughness", {{"metallicFactor", 1.5}}}}}; }), Problem::InvalidValue);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["materials"] = {{{"emissiveFactor", {1, 1}}}}; }), Problem::WrongType);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["samplers"] = {{{"magFilter", 9987}}}; }), Problem::InvalidValue);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["samplers"] = {{{"wrapS", 1}}}; }), Problem::InvalidValue);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["meshes"][0]["primitives"][0]["mode"] = 7; }), Problem::InvalidValue);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["scenes"][0]["nodes"] = {0, 0}; }), Problem::DuplicateElement);
}

// Every object of the file counts against the limit of objects, and the buffers against the limit of bytes.
TEST_F(ReaderRulesTest, RefusesFilesBeyondItsLimits) {
    const std::vector<std::uint8_t> bytes = test::GltfFiles::glb(test::GltfFiles::triangle());
    EXPECT_EQ(failure(bytes, "models/triangle.glb", {.maxObjects = 8})->problem, Problem::LimitExceeded);
    EXPECT_EQ(failure(bytes, "models/triangle.glb", {.maxBufferBytes = 16})->problem, Problem::LimitExceeded);
    EXPECT_EQ(failure(bytes, "models/triangle.glb", {.maxJsonBytes = 64})->problem, Problem::LimitExceeded);

    // An accessor without a buffer view holds zeros, which still count against the bytes of a buffer when it is read.
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["accessors"].push_back({{"componentType", 5126}, {"count", 4000000000ULL}, {"type", "MAT4"}}); }), Problem::LimitExceeded);
}

// Resources lie inside the package: bytes in memory name no file beside them, and URIs hold no scheme, no absolute path and no way out of the content folder.
TEST_F(ReaderRulesTest, RefusesResourcesOutsideThePackage) {
    // clang-format off
    const auto withUri = [](std::string uri) {
        return [uri](test::GltfFiles::File& file) {
            file.document["images"] = {{{"uri", uri}}};
        };
    };
    // clang-format on
    EXPECT_EQ(problemOf(withUri("textures/wood.png")), Problem::FileOutsideDocument);
    EXPECT_EQ(problemOf(withUri("http://example.com/wood.png")), Problem::UriScheme);
    EXPECT_EQ(problemOf(withUri("file:///wood.png")), Problem::UriScheme);
    EXPECT_EQ(problemOf(withUri("data:image/png;base64")), Problem::InvalidUri);
    EXPECT_EQ(problemOf(withUri("data:image/png;base64,====")), Problem::InvalidUri);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["buffers"].push_back({{"byteLength", 4}, {"uri", "data:text/plain;base64,AAAAAA=="}}); }), Problem::InvalidUri);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["buffers"].push_back({{"byteLength", 8}, {"uri", "data:application/octet-stream;base64,AAAAAA=="}}); }), Problem::ResourceTooShort);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["buffers"].push_back({{"byteLength", 4}, {"uri", "data:application/gltf-buffer,%00%01%02%03"}}); }), std::nullopt);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["buffers"].push_back({{"byteLength", 4}}); }), Problem::MissingProperty);
}

// Images name their format when they lie in a buffer view, that format matches their bytes, and a texture of the core takes only PNG and JPEG images.
TEST_F(ReaderRulesTest, RefusesImagesOfTheWrongFormat) {
    // clang-format off
    const auto withImage = [](core::Json image) {
        return [image](test::GltfFiles::File& file) {
            const std::uint32_t offset = test::GltfFiles::append(file.binary, std::array<float, 2>{0.0F, 0.0F});
            std::memcpy(file.binary.data() + offset, "\x89PNG\r\n\x1A\n", 8);
            file.document["buffers"][0]["byteLength"] = file.binary.size();
            file.document["bufferViews"].push_back({{"buffer", 0}, {"byteOffset", offset}, {"byteLength", 8}});
            core::Json entry = image;
            entry["bufferView"] = 3;
            file.document["images"] = core::Json::array({entry});
            file.document["textures"] = {{{"source", 0}}};
        };
    };
    // clang-format on
    EXPECT_EQ(problemOf(withImage({{"mimeType", "image/png"}})), std::nullopt);
    EXPECT_EQ(problemOf(withImage(core::Json::object())), Problem::MissingProperty);
    EXPECT_EQ(problemOf(withImage({{"mimeType", "image/jpeg"}})), Problem::InvalidImage);
    EXPECT_EQ(problemOf(withImage({{"mimeType", "image/webp"}})), Problem::InvalidImage);
    EXPECT_EQ(problemOf(withImage({{"mimeType", "image/png"}, {"uri", "data:image/png;base64,AAAA"}})), Problem::InvalidValue);

    // A buffer view that an image takes as a whole holds no vertices.
    // clang-format off
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["bufferViews"][0].erase("target"); file.document["images"] = core::Json::array({{{"bufferView", 0}, {"mimeType", "image/png"}}}); }),
              // clang-format on
              Problem::UsageConflict);
}

// Buffer views lie inside their buffers with a stride that is a multiple of four from 4 to 252, accessors lie inside their buffer views on the boundaries of their components, and a buffer view holds one kind of data.
TEST_F(ReaderRulesTest, RefusesBrokenLinksBetweenBuffersViewsAndAccessors) {
    // clang-format off
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["bufferViews"][0]["byteLength"] = 400; }), Problem::BufferViewOutOfRange);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["bufferViews"][0]["byteOffset"] = 1ULL << 62U; }), Problem::BufferViewOutOfRange);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["bufferViews"][0]["byteStride"] = 6; }), Problem::InvalidByteStride);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["bufferViews"][0]["byteStride"] = 256; }), Problem::InvalidValue);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["bufferViews"][0]["byteStride"] = 8; }), Problem::InvalidByteStride);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["accessors"][0]["byteOffset"] = 2; }), Problem::Misaligned);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["accessors"][0]["count"] = 4; }), Problem::AccessorOutOfRange);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["accessors"][0]["count"] = 1ULL << 40U; }), Problem::LimitExceeded);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["accessors"][1].erase("bufferView"); file.document["accessors"][1]["byteOffset"] = 0; }),
              Problem::InvalidValue);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["bufferViews"][2]["byteStride"] = 4; file.document["accessors"][2]["count"] = 2; file.document["meshes"][0]["primitives"][0]["mode"] = 0; }),
              Problem::InvalidByteStride);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["bufferViews"][2]["target"] = 34962; }), Problem::UsageConflict);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["bufferViews"][0]["target"] = 34963; }), Problem::UsageConflict);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["accessors"][1]["bufferView"] = 0; }), Problem::InvalidByteStride);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["accessors"][2]["bufferView"] = 0; }), Problem::UsageConflict);
    // clang-format on
}

// Attributes take the formats of their semantics, follow the names of glTF or start with an underscore, number their sets without a gap, pair joints with weights and hold one count of vertices, and indices are unsigned scalars.
TEST_F(ReaderRulesTest, RefusesAttributesThatBreakTheirRules) {
    // clang-format off
    const auto attribute = [](std::string name, std::size_t accessor) {
        return [name, accessor](test::GltfFiles::File& file) {
            file.document["meshes"][0]["primitives"][0]["attributes"][name] = accessor;
        };
    };
    // clang-format on
    EXPECT_EQ(problemOf(attribute("TEXCOORD_0", 1)), Problem::AccessorFormat);
    EXPECT_EQ(problemOf(attribute("TEXCOORD_01", 1)), Problem::InvalidAttribute);
    EXPECT_EQ(problemOf(attribute("COLOUR_0", 1)), Problem::InvalidAttribute);
    EXPECT_EQ(problemOf(attribute("COLOR_1", 1)), Problem::InvalidAttribute);
    EXPECT_EQ(problemOf(attribute("COLOR_0", 1)), std::nullopt);
    EXPECT_EQ(problemOf(attribute("_DATA", 1)), std::nullopt);
    EXPECT_EQ(problemOf(attribute("_DATA", 2)), Problem::Misaligned);
    EXPECT_EQ(problemOf(attribute("NORMAL", 2)), Problem::AccessorFormat);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["accessors"][0].erase("min"); }), Problem::MissingProperty);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["accessors"][1]["count"] = 2; }), Problem::AttributeCount);
    // clang-format off
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["accessors"][2]["type"] = "VEC2"; file.document["accessors"][2]["count"] = 1; }),
              // clang-format on
              Problem::AccessorFormat);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["meshes"][0]["primitives"][0]["attributes"] = core::Json::object(); }), Problem::EmptyElement);
    // clang-format off
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["materials"] = {{{"normalTexture", {{"index", 0}, {"texCoord", 1}}}}}; file.document["textures"] = core::Json::array({core::Json::object()}); file.document["meshes"][0]["primitives"][0]["material"] = 0; }),
              // clang-format on
              Problem::InvalidAttribute);
}

// The count of vertices fits the mode that joins them.
TEST_F(ReaderRulesTest, RefusesVertexCountsThatTheModeCannotJoin) {
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["meshes"][0]["primitives"][0]["mode"] = 1; }), Problem::VertexCount);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["meshes"][0]["primitives"][0]["mode"] = 6; }), std::nullopt);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["meshes"][0]["primitives"][0]["mode"] = 0; }), std::nullopt);
}

// Morph targets move attributes the primitive has, every primitive of a mesh holds as many, and default and node weights give one weight for each.
TEST_F(ReaderRulesTest, RefusesMorphTargetsAndWeightsThatDoNotMatch) {
    // clang-format off
    const auto target = [](core::Json targets) {
        return [targets](test::GltfFiles::File& file) {
            file.document["meshes"][0]["primitives"][0]["targets"] = targets;
        };
    };
    // clang-format on
    EXPECT_EQ(problemOf(target({{{"NORMAL", 1}}})), std::nullopt);
    EXPECT_EQ(problemOf(target({{{"POSITION", 1}}})), Problem::MissingProperty);
    EXPECT_EQ(problemOf(target({{{"TANGENT", 1}}})), Problem::MorphTargets);
    EXPECT_EQ(problemOf(target({{{"POSITION", 2}}})), Problem::AccessorFormat);
    // clang-format off
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["meshes"][0]["primitives"][0]["targets"] = {{{"NORMAL", 1}}}; file.document["meshes"][0]["weights"] = {0.5, 0.5}; }),
              // clang-format on
              Problem::WeightsCount);
    // clang-format off
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["meshes"][0]["primitives"][0]["targets"] = {{{"NORMAL", 1}}}; file.document["nodes"][0]["weights"] = {0.5, 0.5}; }),
              // clang-format on
              Problem::WeightsCount);
    // clang-format off
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["meshes"][0]["primitives"].push_back(file.document["meshes"][0]["primitives"][0]); file.document["meshes"][0]["primitives"][0]["targets"] = {{{"NORMAL", 1}}, {{"NORMAL", 1}}}; file.document["meshes"][0]["primitives"][1]["targets"] = {{{"NORMAL", 1}}}; }),
              // clang-format on
              Problem::MorphTargets);
}

// Nodes form trees: a node has one parent at most, is never its own ancestor and roots a scene only when it has no parent, and a node gives a matrix or translation, rotation and scale, never both, with a rotation of unit length and a matrix that splits into them.
TEST_F(ReaderRulesTest, RefusesNodesThatBreakTheHierarchy) {
    // clang-format off
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["nodes"] = {{{"children", {1}}}, {{"mesh", 0}}}; file.document["scenes"] = {{{"nodes", {1}}}}; }),
              Problem::SceneRoot);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["nodes"] = {{{"children", {1}}}, {{"children", {0}}}}; file.document.erase("scenes"); file.document.erase("scene"); }),
              Problem::NodeCycle);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["nodes"] = {{{"children", {0}}}}; file.document.erase("scenes"); file.document.erase("scene"); }),
              Problem::NodeCycle);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["nodes"] = {{{"children", {2}}}, {{"children", {2}}}, core::Json::object()}; file.document["scenes"] = {{{"nodes", {0, 1}}}}; }),
              Problem::NodeParent);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["nodes"][0]["matrix"] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; file.document["nodes"][0]["scale"] = {1, 1, 1}; }),
              Problem::NodeTransform);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["nodes"][0]["matrix"] = {1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; }), Problem::NodeTransform);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["nodes"][0]["matrix"] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0.5, 0, 0, 0, 1}; }), Problem::NodeTransform);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["nodes"][0]["matrix"] = {0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; }), Problem::NodeTransform);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["nodes"][0]["matrix"] = {2, 0, 0, 0, 0, 2, 0, 0, 0, 0, -2, 0, 5, 6, 7, 1}; }), std::nullopt);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["nodes"][0]["rotation"] = {0, 0, 0, 0.9}; }), Problem::NotUnit);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["nodes"][0]["rotation"] = {0, 0, 0, 1.5}; }), Problem::InvalidValue);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["nodes"][0]["skin"] = 0; file.document["nodes"][0].erase("mesh"); file.document["skins"] = {{{"joints", {0}}}}; }),
              Problem::SkinMesh);
    // clang-format on
}

// A skin names unique joints of one hierarchy, a skeleton that is their ancestor, inverse bind matrices of four by four floats for every joint, and bends only meshes with joints and weights.
TEST_F(ReaderRulesTest, RefusesSkinsThatBreakTheirRules) {
    // clang-format off
    const auto skinned = [](core::Json skin, core::Json nodes) {
        return [skin, nodes](test::GltfFiles::File& file) {
            file.document["nodes"] = nodes;
            file.document["skins"] = core::Json::array({skin});
            file.document["scenes"] = {{{"nodes", {0}}}};
        };
    };
    // clang-format on
    const core::Json pair = {{{"children", {1, 2}}}, core::Json::object(), core::Json::object()};
    EXPECT_EQ(problemOf(skinned({{"joints", {1, 1}}}, pair)), Problem::DuplicateElement);
    EXPECT_EQ(problemOf(skinned({{"joints", core::Json::array()}}, pair)), Problem::EmptyElement);
    EXPECT_EQ(problemOf(skinned({{"joints", {1, 2}}, {"skeleton", 1}}, pair)), Problem::SkinHierarchy);
    EXPECT_EQ(problemOf(skinned({{"joints", {1, 2}}, {"skeleton", 0}}, pair)), std::nullopt);
    EXPECT_EQ(problemOf(skinned({{"joints", {0, 3}}}, {{{"children", {1, 2}}}, core::Json::object(), core::Json::object(), core::Json::object()})), Problem::SkinHierarchy);
    EXPECT_EQ(problemOf(skinned({{"joints", {1, 2}}, {"inverseBindMatrices", 0}}, pair)), Problem::AccessorFormat);
    EXPECT_EQ(problemOf(skinned({{"joints", {1}}}, {{{"children", {1}}}, {{"mesh", 0}, {"skin", 0}}})), Problem::SkinMesh);
}

// Channels animate a property of a node once in each animation, never a node with a matrix, with times of floats that have bounds, values of the format of their path and one value for each keyframe.
TEST_F(ReaderRulesTest, RefusesAnimationsThatBreakTheirRules) {
    // clang-format off
    const auto animated = [](core::Json sampler, core::Json channels, core::Json node) {
        return [sampler, channels, node](test::GltfFiles::File& file) {
            const std::uint32_t times = test::GltfFiles::append(file.binary, std::array<float, 2>{0.0F, 1.0F});
            const std::uint32_t values = test::GltfFiles::append(file.binary, std::array<float, 6>{0.0F, 0.0F, 0.0F, 1.0F, 1.0F, 1.0F});
            file.document["buffers"][0]["byteLength"] = file.binary.size();
            file.document["bufferViews"].push_back({{"buffer", 0}, {"byteOffset", times}, {"byteLength", 8}});
            file.document["bufferViews"].push_back({{"buffer", 0}, {"byteOffset", values}, {"byteLength", 24}});
            file.document["accessors"].push_back({{"bufferView", 3}, {"componentType", 5126}, {"count", 2}, {"type", "SCALAR"}, {"min", {0.0}}, {"max", {1.0}}});
            file.document["accessors"].push_back({{"bufferView", 4}, {"componentType", 5126}, {"count", 2}, {"type", "VEC3"}});
            file.document["nodes"].push_back(node);
            file.document["animations"] = {{{"samplers", {sampler}}, {"channels", channels}}};
        };
    };
    // clang-format on
    const core::Json sampler = {{"input", 3}, {"output", 4}};
    const core::Json translation = {{{"sampler", 0}, {"target", {{"node", 1}, {"path", "translation"}}}}};
    EXPECT_EQ(problemOf(animated(sampler, translation, core::Json::object())), std::nullopt);
    EXPECT_EQ(problemOf(animated(sampler, {translation[0], translation[0]}, core::Json::object())), Problem::AnimationTarget);
    EXPECT_EQ(problemOf(animated(sampler, translation, {{"matrix", {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}}})), Problem::AnimationTarget);
    EXPECT_EQ(problemOf(animated(sampler, {{{"sampler", 0}, {"target", {{"node", 1}, {"path", "rotation"}}}}}, core::Json::object())), Problem::AccessorFormat);
    EXPECT_EQ(problemOf(animated(sampler, {{{"sampler", 0}, {"target", {{"node", 1}, {"path", "weights"}}}}}, core::Json::object())), Problem::AnimationTarget);
    EXPECT_EQ(problemOf(animated({{"input", 3}, {"output", 4}, {"interpolation", "CUBICSPLINE"}}, translation, core::Json::object())), Problem::AccessorFormat);
    EXPECT_EQ(problemOf(animated({{"input", 3}, {"output", 4}, {"interpolation", "SMOOTH"}}, translation, core::Json::object())), Problem::InvalidValue);
    EXPECT_EQ(problemOf(animated({{"input", 4}, {"output", 4}}, translation, core::Json::object())), Problem::AccessorFormat);
    EXPECT_EQ(problemOf(animated(sampler, {{{"sampler", 1}, {"target", {{"node", 1}, {"path", "translation"}}}}}, core::Json::object())), Problem::InvalidIndex);

    // A path that glTF 2.0 does not define belongs to an extension, whose channel stays out of the channels of the core.
    test::GltfFiles::File file = test::GltfFiles::triangle();
    animated(sampler, {{{"sampler", 0}, {"target", {{"path", "pointer"}}}}}, core::Json::object())(file);
    EXPECT_TRUE(Reader::read(test::GltfFiles::glb(file), "models/triangle.glb").animations.at(0).channels.empty());
}

// Cameras have the projection of their type with planes and sizes that the specification allows.
TEST_F(ReaderRulesTest, RefusesCamerasThatBreakTheirRules) {
    // clang-format off
    const auto camera = [](core::Json value) {
        return [value](test::GltfFiles::File& file) {
            file.document["cameras"] = core::Json::array({value});
        };
    };
    // clang-format on
    EXPECT_EQ(problemOf(camera({{"type", "perspective"}, {"perspective", {{"yfov", 1}, {"znear", 0.1}}}})), std::nullopt);
    EXPECT_EQ(problemOf(camera({{"type", "perspective"}, {"perspective", {{"yfov", 1}, {"znear", 0}}}})), Problem::InvalidCamera);
    EXPECT_EQ(problemOf(camera({{"type", "perspective"}, {"perspective", {{"yfov", 1}, {"znear", 1}, {"zfar", 1}}}})), Problem::InvalidCamera);
    EXPECT_EQ(problemOf(camera({{"type", "perspective"}})), Problem::MissingProperty);
    EXPECT_EQ(problemOf(camera({{"type", "fisheye"}})), Problem::InvalidValue);
    EXPECT_EQ(problemOf(camera({{"type", "orthographic"}, {"orthographic", {{"xmag", 0}, {"ymag", 1}, {"znear", 0}, {"zfar", 1}}}})), Problem::InvalidCamera);
    EXPECT_EQ(problemOf(camera({{"type", "orthographic"}, {"orthographic", {{"xmag", 1}, {"ymag", 1}, {"znear", -1}, {"zfar", 1}}}})), Problem::InvalidCamera);
}

// The data of accessors obeys the rules of its use: finite floats, bounds that match, unit normals, indices that name a vertex and never restart a strip.
TEST_F(ReaderRulesTest, RefusesDataThatBreaksTheRulesOfItsUse) {
    // clang-format off
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { setFloat(file, 36, std::numeric_limits<float>::quiet_NaN()); }), Problem::NonFinite);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { setFloat(file, 0, std::numeric_limits<float>::infinity()); }), Problem::NonFinite);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["accessors"][0]["max"] = {1, 1, 0.5}; }), Problem::BoundsMismatch);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.document["accessors"][0]["min"] = {0, -0.1, 0}; }), Problem::BoundsMismatch);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { setFloat(file, 36 + 8, 0.9F); }), Problem::NotUnit);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { setFloat(file, 36 + 8, 0.995F); }), std::nullopt);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.binary[72] = 3; }), Problem::IndexOutOfRange);
    EXPECT_EQ(problemOf([](test::GltfFiles::File& file) { file.binary[72] = 0xFF; file.binary[73] = 0xFF; }),
              Problem::PrimitiveRestart);
    EXPECT_EQ(messageOf([](test::GltfFiles::File& file) { file.binary[74] = 3; }), "The glTF file \"models/triangle.glb\" is not valid, since element 1 of accessor 2, the indices of primitive 0 of mesh 0, names vertex 3 of the 3 vertices of the primitive. Export the model again from the tool that made it, so it follows glTF 2.0.");
    // clang-format on
}

// Sparse indices grow from one to the next and stay below the count of their accessor.
TEST_F(ReaderRulesTest, RefusesSparseIndicesThatDoNotGrow) {
    // clang-format off
    const auto sparse = [](std::array<std::uint16_t, 2> indices) {
        return [indices](test::GltfFiles::File& file) {
            const std::uint32_t at = test::GltfFiles::append(file.binary, indices);
            const std::uint32_t values = test::GltfFiles::append(file.binary, std::array<float, 6>{});
            file.document["buffers"][0]["byteLength"] = file.binary.size();
            file.document["bufferViews"].push_back({{"buffer", 0}, {"byteOffset", at}, {"byteLength", 4}});
            file.document["bufferViews"].push_back({{"buffer", 0}, {"byteOffset", values}, {"byteLength", 24}});
            file.document["accessors"].push_back({{"componentType", 5126}, {"count", 3}, {"type", "VEC3"}, {"sparse", {{"count", 2}, {"indices", {{"bufferView", 3}, {"componentType", 5123}}}, {"values", {{"bufferView", 4}}}}}});
        };
    };
    // clang-format on
    EXPECT_EQ(problemOf(sparse({0, 2})), std::nullopt);
    EXPECT_EQ(problemOf(sparse({2, 1})), Problem::InvalidSparseIndices);
    EXPECT_EQ(problemOf(sparse({0, 3})), Problem::InvalidSparseIndices);
}

} // namespace haylen::gltf
