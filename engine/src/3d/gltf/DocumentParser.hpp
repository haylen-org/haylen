#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "3d/gltf/Diagnostics.hpp"
#include "haylen/3d/gltf/Document.hpp"
#include "haylen/3d/gltf/Limits.hpp"
#include "haylen/core/Json.hpp"

namespace haylen::gltf {

// Turns the JSON of a glTF file into a document and checks every rule that one object of the file obeys on its own: the keys it needs, the types, ranges and enums of its values, the indices it names, which must point at objects of the file, the extensions it uses, which the file must declare, and the URIs of its resources. The rules between objects belong to `LinkValidator`.
class DocumentParser final {
  public:
    // What the container tells about the document: whether it came in a GLB file, how long its binary chunk is, the folder of the file in the content folder and whether its resources may lie in files beside it.
    struct Source {
        bool glb = false;
        std::optional<std::uint64_t> binaryLength;
        std::string directory;
        bool filesAllowed = true;
    };

    [[nodiscard]] static Document parse(const core::Json& root, const Source& source, const Limits& limits, const Diagnostics& diagnostics);

  private:
    // The counts of the lists of the document, which the indices of every object are checked against before the lists are read.
    struct Counts {
        std::size_t accessors = 0;
        std::size_t animations = 0;
        std::size_t buffers = 0;
        std::size_t bufferViews = 0;
        std::size_t cameras = 0;
        std::size_t images = 0;
        std::size_t materials = 0;
        std::size_t meshes = 0;
        std::size_t nodes = 0;
        std::size_t samplers = 0;
        std::size_t scenes = 0;
        std::size_t skins = 0;
        std::size_t textures = 0;
    };

    DocumentParser(const core::Json& documentRoot, const Source& documentSource, const Limits& readerLimits, const Diagnostics& documentDiagnostics);

    void readAsset();
    void readExtensionLists();
    void readBuffers();
    void readBufferViews();
    void readAccessors();
    void readImages();
    void readSamplers();
    void readTextures();
    void readMaterials();
    void readMeshes();
    void readNodes();
    void readSkins();
    void readCameras();
    void readAnimations();
    void readScenes();

    [[nodiscard]] Accessor::Sparse readSparse(const core::Json& object, const std::string& owner);
    [[nodiscard]] std::optional<Material::TextureInfo> readTextureInfo(const core::Json& object, std::string_view key, const std::string& owner);
    [[nodiscard]] Mesh::Primitive readPrimitive(const core::Json& object, const std::string& owner);
    [[nodiscard]] std::vector<Mesh::Attribute> readAttributes(const core::Json& object, const std::string& owner);
    void readNodeTransform(const core::Json& object, const std::string& owner, Node& node);
    [[nodiscard]] Animation readAnimation(const core::Json& object, const std::string& owner);

    // Returns the objects of a list of an object, which must be a non-empty array of objects when the object has it or the list is required, and counts them against the limit of objects.
    [[nodiscard]] std::vector<const core::Json*> readList(const core::Json& owner, std::string_view key, std::string_view itemKind, const std::string& ownerName, bool required);
    void countObjects(std::size_t count);

    [[nodiscard]] const core::Json* find(const core::Json& object, std::string_view key) const;
    [[nodiscard]] const core::Json& require(const core::Json& object, std::string_view key, const std::string& owner) const;
    [[nodiscard]] const core::Json* findObject(const core::Json& object, std::string_view key, const std::string& owner) const;
    [[nodiscard]] std::string readString(const core::Json& object, std::string_view key, const std::string& owner) const;
    [[nodiscard]] std::optional<std::string> readOptionalString(const core::Json& object, std::string_view key, const std::string& owner) const;
    [[nodiscard]] bool readBool(const core::Json& object, std::string_view key, const std::string& owner) const;
    [[nodiscard]] std::uint64_t readInteger(const core::Json& value, std::string_view key, const std::string& owner, std::uint64_t minimum, std::uint64_t maximum) const;
    [[nodiscard]] std::optional<std::uint64_t> readOptionalInteger(const core::Json& object, std::string_view key, const std::string& owner, std::uint64_t minimum, std::uint64_t maximum) const;
    [[nodiscard]] std::size_t readIndex(const core::Json& object, std::string_view key, const std::string& owner, std::size_t count, std::string_view kind) const;
    void requireIndex(std::uint64_t index, std::string_view key, const std::string& owner, std::size_t count, std::string_view kind) const;
    [[nodiscard]] std::optional<std::size_t> readOptionalIndex(const core::Json& object, std::string_view key, const std::string& owner, std::size_t count, std::string_view kind) const;
    [[nodiscard]] std::vector<std::size_t> readIndices(const core::Json& object, std::string_view key, const std::string& owner, std::size_t count, std::string_view kind);
    [[nodiscard]] double readNumber(const core::Json& value, std::string_view key, const std::string& owner) const;
    [[nodiscard]] float readFloat(const core::Json& value, std::string_view key, const std::string& owner) const;
    [[nodiscard]] std::optional<float> readOptionalFloat(const core::Json& object, std::string_view key, const std::string& owner) const;
    [[nodiscard]] std::vector<float> readFloats(const core::Json& object, std::string_view key, const std::string& owner, std::optional<std::size_t> size);
    [[nodiscard]] core::Json readExtras(const core::Json& object) const;
    [[nodiscard]] core::Json readExtensions(const core::Json& object, const std::string& owner) const;

    // Fails for a value outside a range that includes its ends.
    void requireRange(double value, double minimum, double maximum, std::string_view key, const std::string& owner) const;
    [[noreturn]] void wrongType(std::string_view key, const std::string& owner, std::string_view expected) const;
    [[nodiscard]] static std::string_view describeType(const core::Json& value) noexcept;
    [[nodiscard]] bool isUsed(std::string_view extension) const noexcept;

    const core::Json& root;
    const Source& source;
    const Limits& limits;
    const Diagnostics& diagnostics;
    Document document;
    Counts counts;
    std::size_t objects = 0;
};

} // namespace haylen::gltf
