#include "haylen/2d/nodes/NodeSerializer.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "graphics/TextureResource.hpp"
#include "haylen/2d/nodes/NodeTree.hpp"
#include "haylen/2d/particles/System.hpp"
#include "haylen/text/Font.hpp"

namespace haylen::nodes2d {

std::string NodeSerializer::describe(std::string_view node, std::string_view part, std::string_view key) {
    std::string where = "The key \"" + std::string(key) + "\"";
    if (!part.empty()) {
        where += " of the " + std::string(part);
    }
    return where + " of the node \"" + std::string(node) + "\"";
}

void NodeSerializer::requireKeys(const core::Json& object, std::string_view node, std::string_view part, std::initializer_list<std::span<const std::string_view>> allowed) {
    if (!object.is_object()) {
        throw std::invalid_argument(part.empty() ? "The description of the node \"" + std::string(node) + "\" must be an object." : "The " + std::string(part) + " of the node \"" + std::string(node) + "\" must be an object.");
    }
    for (const auto& [key, value] : object.items()) {
        const bool known = std::ranges::any_of(allowed, [&key](std::span<const std::string_view> names) { return std::ranges::find(names, key) != names.end(); });
        if (!known) {
            throw std::invalid_argument(describe(node, part, key) + " is unknown.");
        }
    }
}

float NodeSerializer::number(const core::Json& object, std::string_view key, float fallback, std::string_view node, std::string_view part) {
    const auto found = object.find(key);
    if (found == object.end()) {
        return fallback;
    }
    if (!found->is_number()) {
        throw std::invalid_argument(describe(node, part, key) + " must be a number.");
    }
    return found->get<float>();
}

bool NodeSerializer::flag(const core::Json& object, std::string_view key, bool fallback, std::string_view node, std::string_view part) {
    const auto found = object.find(key);
    if (found == object.end()) {
        return fallback;
    }
    if (!found->is_boolean()) {
        throw std::invalid_argument(describe(node, part, key) + " must be true or false.");
    }
    return found->get<bool>();
}

std::string NodeSerializer::text(const core::Json& object, std::string_view key, std::string_view node, std::string_view part) {
    const auto found = object.find(key);
    if (found == object.end()) {
        return {};
    }
    if (!found->is_string()) {
        throw std::invalid_argument(describe(node, part, key) + " must be a string.");
    }
    return found->get<std::string>();
}

math::Color NodeSerializer::color(const core::Json& object, std::string_view key, math::Color fallback, std::string_view node, std::string_view part) {
    const auto found = object.find(key);
    if (found == object.end()) {
        return fallback;
    }
    const std::optional<math::Color> parsed = found->is_string() ? math::Color::parse(found->get<std::string>()) : std::nullopt;
    if (!parsed) {
        throw std::invalid_argument(describe(node, part, key) + " must be a color such as \"#FFFFFFFF\".");
    }
    return *parsed;
}

math::Vec2 NodeSerializer::pair(const core::Json& object, std::string_view key, math::Vec2 fallback, std::string_view node, std::string_view part) {
    const auto found = object.find(key);
    if (found == object.end()) {
        return fallback;
    }
    if (!found->is_array() || found->size() != 2 || !(*found)[0].is_number() || !(*found)[1].is_number()) {
        throw std::invalid_argument(describe(node, part, key) + " must be a list of two numbers.");
    }
    return {(*found)[0].get<float>(), (*found)[1].get<float>()};
}

math::Rect NodeSerializer::rect(const core::Json& object, std::string_view key, std::string_view node, std::string_view part) {
    const auto found = object.find(key);
    if (found == object.end()) {
        return {};
    }
    const bool numbers = found->is_array() && found->size() == 4 && std::ranges::all_of(*found, [](const core::Json& value) { return value.is_number(); });
    if (!numbers) {
        throw std::invalid_argument(describe(node, part, key) + " must be a list of four numbers: x, y, width and height.");
    }
    return {(*found)[0].get<float>(), (*found)[1].get<float>(), (*found)[2].get<float>(), (*found)[3].get<float>()};
}

graphics::Texture::Options NodeSerializer::textureOptions(const core::Json& object, std::string_view node, std::string_view part) {
    graphics::Texture::Options options;
    if (const std::string filter = text(object, "filter", node, part); !filter.empty()) {
        const auto found = graphics::Texture::filterFromName(filter);
        if (!found) {
            throw std::invalid_argument(describe(node, part, "filter") + " must be \"nearest\" or \"linear\".");
        }
        options.filter = *found;
    }
    if (const std::string wrap = text(object, "wrap", node, part); !wrap.empty()) {
        const auto found = graphics::Texture::wrapFromName(wrap);
        if (!found) {
            throw std::invalid_argument(describe(node, part, "wrap") + " must be \"clamp\", \"repeat\" or \"mirror\".");
        }
        options.wrap = *found;
    }
    return options;
}

void NodeSerializer::validate(const core::Json& description) {
    validateNode(description, text(description.is_object() ? description : core::Json::object(), "name", "", ""));
}

// Reading the properties without assets checks every value of the node, and the children follow.
void NodeSerializer::validateNode(const core::Json& description, const std::string& path) {
    (void)readProperties(description, path, {});
    const auto children = description.find("children");
    if (children == description.end()) {
        return;
    }
    if (!children->is_array()) {
        throw std::invalid_argument(describe(path, {}, "children") + " must be a list of descriptions.");
    }
    for (const core::Json& child : *children) {
        validateNode(child, path + "/" + text(child.is_object() ? child : core::Json::object(), "name", path, {}));
    }
}

NodeProperties NodeSerializer::readProperties(const core::Json& description, const std::string& path, const Assets& assets) {
    requireKeys(description, path, {}, {kNodeKeys, kContentKeys});
    NodeProperties properties{
        .name = text(description, "name", path),
        .position = {number(description, "x", 0.0F, path), number(description, "y", 0.0F, path)},
        .rotation = number(description, "rotation", 0.0F, path),
        .scale = {number(description, "scaleX", 1.0F, path), number(description, "scaleY", 1.0F, path)},
        .color = color(description, "color", math::Color::white(), path),
        .visible = flag(description, "visible", true, path),
        .layer = static_cast<int>(number(description, "layer", 0.0F, path)),
        .depth = number(description, "depth", 0.0F, path),
        .sortOffset = number(description, "sortOffset", 0.0F, path),
    };
    properties.content = readContent(description, path, assets);
    return properties;
}

// Without assets the content is only checked, which `validate` relies on.
NodeContent NodeSerializer::readContent(const core::Json& description, const std::string& path, const Assets& assets) {
    const auto shown = std::ranges::count_if(std::array<std::string_view, 4>{"sprite", "text", "frame", "particles"}, [&description](std::string_view key) { return description.contains(key); });
    if (shown > 1) {
        throw std::invalid_argument("The node \"" + path + "\" shows more than one of \"sprite\", \"text\", \"frame\" and \"particles\".");
    }
    const bool resolve = static_cast<bool>(assets.texture);

    if (const auto found = description.find("sprite"); found != description.end()) {
        requireKeys(*found, path, "sprite", {kSpriteKeys});
        const std::string texture = text(*found, "texture", path, "sprite");
        if (texture.empty()) {
            throw std::invalid_argument("The sprite of the node \"" + path + "\" needs the path of its texture.");
        }
        graphics2d::Sprite sprite{
            .source = rect(*found, "source", path, "sprite"),
            .position = {number(*found, "x", 0.0F, path, "sprite"), number(*found, "y", 0.0F, path, "sprite")},
            .size = {number(*found, "width", 0.0F, path, "sprite"), number(*found, "height", 0.0F, path, "sprite")},
            .scale = {number(*found, "scaleX", 1.0F, path, "sprite"), number(*found, "scaleY", 1.0F, path, "sprite")},
            .pivot = {number(*found, "pivotX", 0.5F, path, "sprite"), number(*found, "pivotY", 0.5F, path, "sprite")},
            .rotation = number(*found, "rotation", 0.0F, path, "sprite"),
            .color = color(*found, "color", math::Color::white(), path, "sprite"),
            .flip = {.horizontal = flag(*found, "flipHorizontal", false, path, "sprite"), .vertical = flag(*found, "flipVertical", false, path, "sprite")},
            .order = {.layer = static_cast<int>(number(*found, "layer", 0.0F, path, "sprite"))},
        };
        const graphics::Texture::Options options = textureOptions(*found, path, "sprite");
        if (!resolve) {
            return {};
        }
        sprite.texture = assets.texture(texture, options);
        return {.value = std::move(sprite)};
    }

    if (const auto found = description.find("text"); found != description.end()) {
        requireKeys(*found, path, "text", {kTextKeys});
        text::Style style{
            .size = number(*found, "size", 32.0F, path, "text"),
            .color = color(*found, "color", math::Color::white(), path, "text"),
            .outlineWidth = number(*found, "outlineWidth", 0.0F, path, "text"),
            .outlineColor = color(*found, "outlineColor", math::Color::black(), path, "text"),
            .shadowOffset = pair(*found, "shadowOffset", {}, path, "text"),
            .shadowColor = color(*found, "shadowColor", math::Color::transparent(), path, "text"),
            .shadowBlur = number(*found, "shadowBlur", 0.0F, path, "text"),
            .maxWidth = number(*found, "maxWidth", 0.0F, path, "text"),
            .maxHeight = number(*found, "maxHeight", 0.0F, path, "text"),
            .minScale = number(*found, "minScale", 1.0F, path, "text"),
            .wrap = flag(*found, "wrap", true, path, "text"),
            .lineSpacing = number(*found, "lineSpacing", 1.2F, path, "text"),
            .anchor = pair(*found, "anchor", {}, path, "text"),
        };
        if (!(style.minScale > 0.0F && style.minScale <= 1.0F)) {
            throw std::invalid_argument(describe(path, "text", "minScale") + " must be above 0 and at most 1.");
        }
        if (const std::string align = text(*found, "align", path, "text"); !align.empty()) {
            const auto alignment = text::Style::alignmentFromName(align);
            if (!alignment) {
                throw std::invalid_argument(describe(path, "text", "align") + " must be \"start\", \"end\", \"left\", \"center\", \"right\" or \"fill\".");
            }
            style.align = *alignment;
        }
        const std::string font = text(*found, "font", path, "text");
        NodeContent::Text block{.content = text(*found, "text", path, "text"), .style = style, .order = {.layer = static_cast<int>(number(*found, "layer", 0.0F, path, "text"))}};
        if (!resolve) {
            return {};
        }
        block.font = assets.font(font);
        return {.value = std::move(block), .path = font};
    }

    if (const auto found = description.find("frame"); found != description.end()) {
        requireKeys(*found, path, "frame", {kFrameKeys});
        const std::string texture = text(*found, "texture", path, "frame");
        if (texture.empty()) {
            throw std::invalid_argument("The frame of the node \"" + path + "\" needs the path of its texture.");
        }
        const math::Rect borders = rect(*found, "borders", path, "frame");
        NodeContent::Frame frame{
            .size = {number(*found, "width", 0.0F, path, "frame"), number(*found, "height", 0.0F, path, "frame")},
            .pivot = {number(*found, "pivotX", 0.5F, path, "frame"), number(*found, "pivotY", 0.5F, path, "frame")},
            .color = color(*found, "color", math::Color::white(), path, "frame"),
            .borderScale = number(*found, "borderScale", 1.0F, path, "frame"),
            .order = {.layer = static_cast<int>(number(*found, "layer", 0.0F, path, "frame"))},
        };
        const math::Rect source = rect(*found, "source", path, "frame");
        graphics2d::NineSlice::Fill fill = graphics2d::NineSlice::Fill::Stretch;
        if (const std::string name = text(*found, "fill", path, "frame"); !name.empty()) {
            const auto parsed = graphics2d::NineSlice::fillFromName(name);
            if (!parsed) {
                throw std::invalid_argument(describe(path, "frame", "fill") + " must be \"stretch\" or \"tile\".");
            }
            fill = *parsed;
        }
        const graphics::Texture::Options options = textureOptions(*found, path, "frame");
        if (!resolve) {
            return {};
        }
        frame.slice = graphics2d::NineSlice::fromBorders(assets.texture(texture, options), source, {.left = borders.x, .top = borders.y, .right = borders.width, .bottom = borders.height});
        frame.slice.fill = fill;
        return {.value = std::move(frame)};
    }

    if (description.contains("particles")) {
        const std::string effect = text(description, "particles", path);
        if (effect.empty()) {
            throw std::invalid_argument("The particles of the node \"" + path + "\" need the path of their effect file.");
        }
        if (!resolve) {
            return {};
        }
        return {.value = assets.particles(effect), .path = effect};
    }
    return {};
}

Node NodeSerializer::load(NodeTree& tree, const Node& parent, const core::Json& description, const Assets& assets) {
    validate(description);
    return loadNode(tree, parent, description, text(description, "name", "", ""), assets);
}

Node NodeSerializer::loadNode(NodeTree& tree, const Node& parent, const core::Json& description, const std::string& path, const Assets& assets) {
    const NodeProperties properties = readProperties(description, path, assets);
    const Node node = parent.isValid() ? tree.add(parent, properties) : tree.add(properties);
    if (const auto children = description.find("children"); children != description.end()) {
        for (const core::Json& child : *children) {
            (void)loadNode(tree, node, child, path + "/" + text(child, "name", path, {}), assets);
        }
    }
    return node;
}

std::string NodeSerializer::texturePath(const graphics::Texture& texture, std::string_view node) {
    if (texture.getResource()->label.empty()) {
        throw std::invalid_argument("The node \"" + std::string(node) + "\" shows a texture that no package path names, so it cannot be saved.");
    }
    return texture.getResource()->label;
}

void NodeSerializer::writeTexture(core::Json& object, const graphics::Texture& texture, std::string_view node) {
    object["texture"] = texturePath(texture, node);
    const graphics::Texture::Options options = texture.getOptions();
    if (options.filter != graphics::Texture::Filter::Nearest) {
        object["filter"] = graphics::Texture::filterName(options.filter);
    }
    if (options.wrap != graphics::Texture::Wrap::Clamp) {
        object["wrap"] = graphics::Texture::wrapName(options.wrap);
    }
}

core::Json NodeSerializer::save(const Node& node, const std::shared_ptr<text::Font>& defaultFont) {
    core::Json description = core::Json::object();
    // clang-format off
    const auto put = [&description](std::string_view key, auto value, auto fallback) {
        if (value != fallback) {
            description[std::string(key)] = value;
        }
    };
    // clang-format on
    put("name", node.getName(), std::string());
    put("x", node.getX(), 0.0F);
    put("y", node.getY(), 0.0F);
    put("rotation", node.getRotation(), 0.0F);
    put("scaleX", node.getScaleX(), 1.0F);
    put("scaleY", node.getScaleY(), 1.0F);
    if (node.getColor() != math::Color::white()) {
        description["color"] = node.getColor().toHex();
    }
    put("visible", node.isVisible(), true);
    put("layer", node.getLayer(), 0);
    put("depth", node.getDepth(), 0.0F);
    put("sortOffset", node.getSortOffset(), 0.0F);
    writeContent(description, node, defaultFont);

    const std::vector<Node> children = node.getChildren();
    if (!children.empty()) {
        core::Json list = core::Json::array();
        for (const Node& child : children) {
            list.push_back(save(child, defaultFont));
        }
        description["children"] = std::move(list);
    }
    return description;
}

void NodeSerializer::writeContent(core::Json& object, const Node& node, const std::shared_ptr<text::Font>& defaultFont) {
    const NodeContent& content = node.getContent();
    const std::string path = node.getPath();
    core::Json part = core::Json::object();
    // clang-format off
    const auto put = [&part](std::string_view key, auto value, auto fallback) {
        if (value != fallback) {
            part[std::string(key)] = value;
        }
    };
    // clang-format on
    // clang-format off
    const auto putColor = [&part](std::string_view key, math::Color value, math::Color fallback) {
        if (value != fallback) {
            part[std::string(key)] = value.toHex();
        }
    };
    // clang-format on

    if (const auto* sprite = std::get_if<graphics2d::Sprite>(&content.value)) {
        writeTexture(part, sprite->texture, path);
        if (!sprite->source.isEmpty()) {
            part["source"] = {sprite->source.x, sprite->source.y, sprite->source.width, sprite->source.height};
        }
        put("x", sprite->position.x, 0.0F);
        put("y", sprite->position.y, 0.0F);
        put("width", sprite->size.x, 0.0F);
        put("height", sprite->size.y, 0.0F);
        put("scaleX", sprite->scale.x, 1.0F);
        put("scaleY", sprite->scale.y, 1.0F);
        put("pivotX", sprite->pivot.x, 0.5F);
        put("pivotY", sprite->pivot.y, 0.5F);
        put("rotation", sprite->rotation, 0.0F);
        putColor("color", sprite->color, math::Color::white());
        put("flipHorizontal", sprite->flip.horizontal, false);
        put("flipVertical", sprite->flip.vertical, false);
        put("layer", sprite->order.layer, 0);
        object["sprite"] = std::move(part);
    } else if (const auto* shown = std::get_if<NodeContent::Text>(&content.value)) {
        if (shown->font != defaultFont) {
            if (content.path.empty()) {
                throw std::invalid_argument("The node \"" + path + "\" shows text in a font that no package path names, so it cannot be saved.");
            }
            part["font"] = content.path;
        }
        part["text"] = shown->content;
        const text::Style& style = shown->style;
        put("size", style.size, 32.0F);
        putColor("color", style.color, math::Color::white());
        put("outlineWidth", style.outlineWidth, 0.0F);
        putColor("outlineColor", style.outlineColor, math::Color::black());
        if (!style.shadowOffset.isZero()) {
            part["shadowOffset"] = {style.shadowOffset.x, style.shadowOffset.y};
        }
        putColor("shadowColor", style.shadowColor, math::Color::transparent());
        put("shadowBlur", style.shadowBlur, 0.0F);
        if (style.align != text::Alignment::Start) {
            part["align"] = text::Style::alignmentName(style.align);
        }
        put("maxWidth", style.maxWidth, 0.0F);
        put("maxHeight", style.maxHeight, 0.0F);
        put("minScale", style.minScale, 1.0F);
        put("wrap", style.wrap, true);
        put("lineSpacing", style.lineSpacing, 1.2F);
        if (!style.anchor.isZero()) {
            part["anchor"] = {style.anchor.x, style.anchor.y};
        }
        put("layer", shown->order.layer, 0);
        object["text"] = std::move(part);
    } else if (const auto* frame = std::get_if<NodeContent::Frame>(&content.value)) {
        // A frame saves as the source and the borders that cut it, so a frame of separate pieces has no description.
        const math::Rect source = math::Rect::fromMinMax(frame->slice.pieces[0].getMin(), frame->slice.pieces[8].getMax());
        const math::Insets borders = frame->slice.getBorders();
        if (graphics2d::NineSlice::fromBorders(frame->slice.texture, source, borders).pieces != frame->slice.pieces) {
            throw std::invalid_argument("The node \"" + path + "\" shows a frame of separate pieces, which only a source and borders describe, so it cannot be saved.");
        }
        writeTexture(part, frame->slice.texture, path);
        part["source"] = {source.x, source.y, source.width, source.height};
        part["borders"] = {borders.left, borders.top, borders.right, borders.bottom};
        if (frame->slice.fill != graphics2d::NineSlice::Fill::Stretch) {
            part["fill"] = graphics2d::NineSlice::fillName(frame->slice.fill);
        }
        put("width", frame->size.x, 0.0F);
        put("height", frame->size.y, 0.0F);
        put("pivotX", frame->pivot.x, 0.5F);
        put("pivotY", frame->pivot.y, 0.5F);
        putColor("color", frame->color, math::Color::white());
        put("borderScale", frame->borderScale, 1.0F);
        put("layer", frame->order.layer, 0);
        object["frame"] = std::move(part);
    } else if (!content.isEmpty()) {
        if (content.path.empty()) {
            throw std::invalid_argument("The node \"" + path + "\" shows particles that no effect file names, so it cannot be saved.");
        }
        object["particles"] = content.path;
    }
}

} // namespace haylen::nodes2d
