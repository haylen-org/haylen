#pragma once

#include <array>
#include <functional>
#include <initializer_list>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "haylen/2d/nodes/Node.hpp"
#include "haylen/2d/nodes/NodeContent.hpp"
#include "haylen/2d/nodes/NodeProperties.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::text {
class Font;
}

namespace haylen::particles2d {
class System;
}

namespace haylen::nodes2d {

class NodeTree;

// Reads and writes nodes as JSON descriptions: an object with the local state of a node, what it shows and its `children`, the format of `.prefab` files and of saved subtrees. Assets appear by their package paths.
class NodeSerializer final {
  public:
    // Finds the assets that descriptions name. The font of an empty path is the default font, which saving leaves out.
    struct Assets {
        std::function<graphics::Texture(const std::string& path, graphics::Texture::Options options)> texture;
        std::function<std::shared_ptr<text::Font>(const std::string& path)> font;
        std::function<std::shared_ptr<particles2d::System>(const std::string& path)> particles;
    };

    // Checks a description and every description under it, and throws `std::invalid_argument` naming the node, by its path of names, and the key at fault.
    static void validate(const core::Json& description);

    // Builds the node of a description and its descendants under the parent, or at the root for an invalid parent, and returns it.
    static Node load(NodeTree& tree, const Node& parent, const core::Json& description, const Assets& assets);

    // Describes a node and its descendants, leaving out the values that equal their defaults. Throws `std::invalid_argument` for content that no package path names, such as a texture made in code, naming the node.
    [[nodiscard]] static core::Json save(const Node& node, const std::shared_ptr<text::Font>& defaultFont);

  private:
    static constexpr std::array<std::string_view, 13> kNodeKeys{"name", "x", "y", "rotation", "scaleX", "scaleY", "color", "visible", "layer", "depth", "sortOffset", "children", "sprite"};
    static constexpr std::array<std::string_view, 3> kContentKeys{"text", "frame", "particles"};
    static constexpr std::array<std::string_view, 17> kSpriteKeys{"texture", "filter", "wrap", "source", "x", "y", "width", "height", "scaleX", "scaleY", "pivotX", "pivotY", "rotation", "color", "flipHorizontal", "flipVertical", "layer"};
    static constexpr std::array<std::string_view, 17> kTextKeys{"font", "text", "size", "color", "outlineWidth", "outlineColor", "shadowOffset", "shadowColor", "shadowBlur", "align", "maxWidth", "maxHeight", "minScale", "wrap", "lineSpacing", "anchor", "layer"};
    static constexpr std::array<std::string_view, 13> kFrameKeys{"texture", "filter", "wrap", "source", "borders", "fill", "width", "height", "pivotX", "pivotY", "color", "borderScale", "layer"};

    // Throws for the first key of an object that the lists leave out, naming the node.
    static void requireKeys(const core::Json& object, std::string_view node, std::string_view part, std::initializer_list<std::span<const std::string_view>> allowed);
    [[nodiscard]] static std::string describe(std::string_view node, std::string_view part, std::string_view key);
    [[nodiscard]] static float number(const core::Json& object, std::string_view key, float fallback, std::string_view node, std::string_view part = {});
    [[nodiscard]] static bool flag(const core::Json& object, std::string_view key, bool fallback, std::string_view node, std::string_view part = {});
    [[nodiscard]] static std::string text(const core::Json& object, std::string_view key, std::string_view node, std::string_view part = {});
    [[nodiscard]] static math::Color color(const core::Json& object, std::string_view key, math::Color fallback, std::string_view node, std::string_view part = {});
    [[nodiscard]] static math::Vec2 pair(const core::Json& object, std::string_view key, math::Vec2 fallback, std::string_view node, std::string_view part);
    [[nodiscard]] static math::Rect rect(const core::Json& object, std::string_view key, std::string_view node, std::string_view part);
    [[nodiscard]] static graphics::Texture::Options textureOptions(const core::Json& object, std::string_view node, std::string_view part);

    static void validateNode(const core::Json& description, const std::string& path);
    [[nodiscard]] static NodeProperties readProperties(const core::Json& description, const std::string& path, const Assets& assets);
    [[nodiscard]] static NodeContent readContent(const core::Json& description, const std::string& path, const Assets& assets);
    static Node loadNode(NodeTree& tree, const Node& parent, const core::Json& description, const std::string& path, const Assets& assets);

    [[nodiscard]] static std::string texturePath(const graphics::Texture& texture, std::string_view node);
    static void writeTexture(core::Json& object, const graphics::Texture& texture, std::string_view node);
    static void writeContent(core::Json& object, const Node& node, const std::shared_ptr<text::Font>& defaultFont);
};

} // namespace haylen::nodes2d
