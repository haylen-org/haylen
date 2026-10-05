#include "haylen/ui/Style.hpp"

#include <stdexcept>
#include <utility>

#include "haylen/core/JsonValidator.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/ui/PropertyReader.hpp"

namespace haylen::ui {

const core::Json& Style::readSection(const core::Json& value, const char* key, const std::string& subject) {
    static const core::Json& empty = *new const core::Json(core::Json::object());
    const core::Json& section = value.contains(key) ? value.at(key) : empty;
    if (!section.is_object()) {
        throw std::invalid_argument(subject + " must hold a table in \"" + key + "\".");
    }
    return section;
}

// Every error starts with the property that holds the style, and its values follow the rules of theme files. Surfaces are read once their images arrive, since they load when they first draw.
Style Style::fromJson(const core::Json& value, std::string_view path) {
    const std::string subject = PropertyReader::describeProperty(path);
    const std::string inside = "t" + subject.substr(1);
    if (!value.is_object()) {
        throw std::invalid_argument(subject + " must be a table with \"colors\", \"metrics\", \"fonts\" or \"surfaces\".");
    }
    core::JsonValidator::requireKnownKeys(value, {"colors", "metrics", "fonts", "surfaces"}, inside);

    Style style;
    for (const auto& [key, color] : readSection(value, "colors", subject).items()) {
        const std::optional<Theme::Color> role = Theme::colorFromName(key);
        if (!role) {
            throw std::invalid_argument(subject + " has no color role named \"" + key + "\".");
        }
        style.colors[static_cast<std::size_t>(*role)] = Theme::readColor(color, "The color \"" + key + "\" of " + inside);
    }
    for (const auto& [key, metric] : readSection(value, "metrics", subject).items()) {
        const std::optional<Theme::Metric> role = Theme::metricFromName(key);
        if (!role) {
            throw std::invalid_argument(subject + " has no metric named \"" + key + "\".");
        }
        style.metrics[static_cast<std::size_t>(*role)] = Theme::readNumber(metric, "The metric \"" + key + "\" of " + inside);
    }
    readFonts(style, readSection(value, "fonts", subject), subject, inside);
    for (const auto& [key, surface] : readSection(value, "surfaces", subject).items()) {
        const std::optional<Theme::Surface> role = Theme::surfaceFromName(key);
        if (!role) {
            throw std::invalid_argument(subject + " has no surface named \"" + key + "\".");
        }
        const bool flat = surface.is_null() || (surface.is_boolean() && !surface.get<bool>());
        Surface entry{.definition = flat ? core::Json() : surface, .context = "the surface \"" + key + "\" of " + inside, .image = std::nullopt};
        if (!flat) {
            core::JsonValidator::requireKnownKeys(surface, {"image", "source", "slice", "pieces", "scale", "padding", "tint", "colorize", "filter", "fill"}, entry.context);
            if (!surface.contains("image") || !surface.at("image").is_string()) {
                throw std::invalid_argument("The image of " + entry.context + " must be a path.");
            }
        }
        style.surfaces[static_cast<std::size_t>(*role)] = std::move(entry);
    }
    return style;
}

void Style::readFonts(Style& style, const core::Json& section, const std::string& subject, const std::string& inside) {
    for (const auto& [key, value] : section.items()) {
        const std::optional<Theme::Font> role = Theme::fontFromName(key);
        if (!role) {
            throw std::invalid_argument(subject + " has no font role named \"" + key + "\".");
        }
        const std::string font = "the font \"" + key + "\" of " + inside;
        if (!value.is_object()) {
            throw std::invalid_argument("T" + font.substr(1) + " must be a table.");
        }
        core::JsonValidator::requireKnownKeys(value, {"font", "size", "bold", "italic"}, font);
        Font& entry = style.fonts[static_cast<std::size_t>(*role)];
        if (value.contains("font")) {
            if (!value.at("font").is_string() || value.at("font").get_ref<const std::string&>().empty()) {
                throw std::invalid_argument("T" + font.substr(1) + " must name its font with a string.");
            }
            entry.font = value.at("font").get<std::string>();
        }
        if (value.contains("size")) {
            entry.size = Theme::readNumber(value.at("size"), "The size of " + font);
            if (*entry.size <= 0.0F) {
                throw std::invalid_argument("The size of " + font + " must be positive.");
            }
        }
        for (const auto& [flag, field] : {std::pair{"bold", &entry.bold}, std::pair{"italic", &entry.italic}}) {
            if (!value.contains(flag)) {
                continue;
            }
            if (!value.at(flag).is_boolean()) {
                throw std::invalid_argument("T" + font.substr(1) + " must set \"" + flag + "\" to \"true\" or \"false\".");
            }
            *field = value.at(flag).get<bool>();
        }
    }
}

// A surface waits for its texture, so the image is made and kept once the loader hands over a texture that arrived.
const Theme::Image* Style::findSurface(Theme::Surface role, const Theme::TextureLoader& loadTexture) const {
    const std::optional<Surface>& surface = surfaces[static_cast<std::size_t>(role)];
    if (!surface || surface->definition.is_null()) {
        return nullptr;
    }
    if (!surface->image) {
        graphics::Texture::Options options;
        if (const auto filter = surface->definition.find("filter"); filter != surface->definition.end() && filter->is_string()) {
            options.filter = graphics::Texture::filterFromName(filter->get_ref<const std::string&>()).value_or(options.filter);
        }
        if (!loadTexture(surface->definition.at("image").get_ref<const std::string&>(), options).isValid()) {
            return nullptr;
        }
        surface->image = Theme::readImage(surface->definition, surface->context, loadTexture);
    }
    return &*surface->image;
}

} // namespace haylen::ui
