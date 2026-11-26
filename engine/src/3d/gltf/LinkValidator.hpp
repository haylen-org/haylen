#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "3d/gltf/Diagnostics.hpp"
#include "haylen/3d/gltf/Document.hpp"

namespace haylen::gltf {

// Checks the rules of glTF 2.0 between the objects of a document: the formats that each use of an accessor takes, the buffer views that hold one kind of data, the attributes and morph targets of primitives, the hierarchy of nodes and scenes, skins and their joints, and the channels and samplers of animations. It fills the parent of every node.
class LinkValidator final {
  public:
    static void validate(Document& document, const Diagnostics& diagnostics);

  private:
    // The one kind of data that a buffer view used by vertices, indices or inverse bind matrices holds, or the image that takes a buffer view as a whole.
    enum class ViewUse : std::uint8_t {
        None,
        Vertices,
        Indices,
        InverseBindMatrices,
        Image,
        Other,
    };

    // The effective formats that one use of an accessor takes.
    struct Format {
        Accessor::Type type;
        Accessor::ComponentType componentType;
        bool normalized;
    };

    // The semantic of an attribute as the tables of glTF name it, such as `TEXCOORD` with the index 1 for `TEXCOORD_1`.
    struct Semantic {
        std::string base;
        std::uint32_t index = 0;
        bool indexed = false;
    };

    LinkValidator(Document& target, const Diagnostics& documentDiagnostics);

    void validateMeshes();
    void validatePrimitive(const Mesh::Primitive& primitive, const std::string& owner);
    void validateAttributes(const std::vector<Mesh::Attribute>& attributes, const std::string& owner, bool morph);
    void validateNodes();
    void validateSkins();
    void validateAnimations();
    void validateTextures();

    void useView(std::size_t view, ViewUse use, const std::string& owner);
    void useAccessorView(std::size_t accessor, ViewUse use, const std::string& owner);
    void requireFormat(std::size_t accessor, std::initializer_list<Format> formats, const std::string& owner) const;
    void requireNoStrideOrTarget(std::size_t accessor, const std::string& owner, bool allowTarget) const;

    // Returns nothing for an application-specific attribute, whose name starts with an underscore.
    [[nodiscard]] std::optional<Semantic> parseSemantic(std::string_view name, const std::string& owner, bool morph) const;
    // Tells whether a mode joins a count of vertices into whole points, lines or triangles.
    [[nodiscard]] static bool joins(Mesh::Primitive::Mode mode, std::uint64_t count) noexcept;
    [[nodiscard]] std::size_t findRoot(std::size_t node) const noexcept;
    [[nodiscard]] bool isAncestor(std::size_t ancestor, std::size_t node) const noexcept;

    Document& document;
    const Diagnostics& diagnostics;
    std::vector<ViewUse> viewUses;
    std::vector<std::optional<std::size_t>> attributeViewAccessors;
};

} // namespace haylen::gltf
