#include "haylen/ui/Theme.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <imgui.h>

#include "haylen/core/JsonValidator.hpp"
#include "ui/ImGuiConverter.hpp"

namespace haylen::ui {

template <typename Enum, std::size_t Count> std::optional<Enum> Theme::fromName(const std::array<std::string_view, Count>& names, std::string_view value) noexcept {
    const auto found = std::ranges::find(names, value);
    if (found == names.end()) {
        return std::nullopt;
    }
    return static_cast<Enum>(found - names.begin());
}

math::Color Theme::readColor(const core::Json& value, const std::string& context) {
    const std::optional<math::Color> color = value.is_string() ? math::Color::parse(value.get<std::string>()) : std::nullopt;
    if (!color) {
        throw std::invalid_argument(context + " must be a color such as \"#FF2E7D32\".");
    }
    return *color;
}

float Theme::readNumber(const core::Json& value, const std::string& context) {
    if (!value.is_number() || !std::isfinite(value.get<double>()) || value.get<double>() < 0.0) {
        throw std::invalid_argument(context + " must be a non-negative number.");
    }
    return value.get<float>();
}

// Insets are one number for every side, two for the vertical and horizontal sides, or four from the top clockwise.
math::Insets Theme::readInsets(const core::Json& value, const std::string& context) {
    std::vector<float> sides;
    if (value.is_number()) {
        sides.assign(4, readNumber(value, context));
    } else if (value.is_array() && (value.size() == 2 || value.size() == 4)) {
        for (const core::Json& side : value) {
            sides.push_back(readNumber(side, context));
        }
    } else {
        throw std::invalid_argument(context + " must be one, two or four numbers.");
    }
    if (sides.size() == 2) {
        return {.left = sides[1], .top = sides[0], .right = sides[1], .bottom = sides[0]};
    }
    return {.left = sides[3], .top = sides[0], .right = sides[1], .bottom = sides[2]};
}

math::Rect Theme::readRect(const core::Json& value, const std::string& context) {
    if (!value.is_array() || value.size() != 4) {
        throw std::invalid_argument(context + " must be four numbers: x, y, width and height.");
    }
    return {readNumber(value[0], context), readNumber(value[1], context), readNumber(value[2], context), readNumber(value[3], context)};
}

Theme::Image Theme::readImage(const core::Json& value, const std::string& context, const TextureLoader& loadTexture) {
    core::JsonValidator::requireKnownKeys(value, {"image", "source", "slice", "pieces", "scale", "padding", "tint", "colorize", "filter", "fill"}, context);
    if (!value.contains("image") || !value.at("image").is_string()) {
        throw std::invalid_argument("The image of " + context + " must be a path.");
    }

    graphics::Texture::Options options;
    if (value.contains("filter")) {
        const std::optional<graphics::Texture::Filter> filter = value.at("filter").is_string() ? graphics::Texture::filterFromName(value.at("filter").get<std::string>()) : std::nullopt;
        if (!filter) {
            throw std::invalid_argument("The filter of " + context + " must be \"nearest\" or \"linear\".");
        }
        options.filter = *filter;
    }
    const graphics::Texture texture = loadTexture(value.at("image").get<std::string>(), options);

    Image image;
    if (value.contains("pieces")) {
        const core::Json& pieces = value.at("pieces");
        if (!pieces.is_array() || pieces.size() != 9) {
            throw std::invalid_argument("The pieces of " + context + " must hold nine rectangles.");
        }
        std::array<math::Rect, 9> rects{};
        for (std::size_t index = 0; index < rects.size(); ++index) {
            rects[index] = readRect(pieces[index], "Each piece of " + context);
        }
        image.slice = graphics2d::NineSlice::fromPieces(texture, rects);
    } else {
        const math::Rect source = value.contains("source") ? readRect(value.at("source"), "The source of " + context) : math::Rect{0.0F, 0.0F, texture.getSize().x, texture.getSize().y};
        const math::Insets borders = value.contains("slice") ? readInsets(value.at("slice"), "The slice of " + context) : math::Insets{};
        image.slice = graphics2d::NineSlice::fromBorders(texture, source, borders);
    }
    if (value.contains("fill")) {
        const core::Json& fill = value.at("fill");
        const std::optional<graphics2d::NineSlice::Fill> named = fill.is_string() ? graphics2d::NineSlice::fillFromName(fill.get<std::string>()) : std::nullopt;
        if (!named) {
            throw std::invalid_argument("The fill of " + context + " must be \"stretch\" or \"tile\".");
        }
        image.slice.fill = *named;
    }
    if (value.contains("scale")) {
        image.scale = readNumber(value.at("scale"), "The scale of " + context);
        if (image.scale <= 0.0F) {
            throw std::invalid_argument("The scale of " + context + " must be positive.");
        }
    }
    if (value.contains("padding")) {
        image.padding = readInsets(value.at("padding"), "The padding of " + context);
    }
    if (value.contains("tint")) {
        image.tint = readColor(value.at("tint"), "The tint of " + context);
    }
    if (value.contains("colorize")) {
        if (!value.at("colorize").is_boolean()) {
            throw std::invalid_argument("The \"colorize\" of " + context + " must be \"true\" or \"false\".");
        }
        image.colorize = value.at("colorize").get<bool>();
    }
    if (image.slice.fill == graphics2d::NineSlice::Fill::Tile) {
        checkTiles(image, context);
    }
    return image;
}

// Every copy of a tiled piece covers at least one design unit, so tiling always ends and never draws an unbounded number of copies. A piece without area draws nothing and needs no check.
void Theme::checkTiles(const Image& image, const std::string& context) {
    for (const std::size_t index : kTiledPieces) {
        const math::Rect& piece = image.slice.pieces[index];
        const bool drawn = piece.width > 0.0F && piece.height > 0.0F;
        if (drawn && (piece.width * image.scale < kMinTileSize || piece.height * image.scale < kMinTileSize)) {
            throw std::invalid_argument("The tiled edges and center of " + context + " must each be at least 1 unit wide and tall at its scale.");
        }
    }
}

const core::Json& Theme::readSection(const core::Json& document, const char* key) {
    static const core::Json& empty = *new const core::Json(core::Json::object());
    const core::Json& value = document.contains(key) ? document.at(key) : empty;
    if (!value.is_object()) {
        throw std::invalid_argument(std::string("The section \"") + key + "\" of the theme must be an object.");
    }
    return value;
}

std::optional<Theme::Color> Theme::colorFromName(std::string_view value) noexcept {
    return fromName<Color>(kColorNames, value);
}

std::optional<Theme::Metric> Theme::metricFromName(std::string_view value) noexcept {
    return fromName<Metric>(kMetricNames, value);
}

std::optional<Theme::Font> Theme::fontFromName(std::string_view value) noexcept {
    return fromName<Font>(kFontNames, value);
}

std::optional<Theme::Surface> Theme::surfaceFromName(std::string_view value) noexcept {
    return fromName<Surface>(kSurfaceNames, value);
}

Theme Theme::dark() {
    Theme theme;
    theme.name = "dark";
    for (std::size_t index = 0; index < kColorCount; ++index) {
        theme.colors[index] = math::Color::fromHex(kDarkPalette[index]);
    }
    theme.metrics = kMetrics;
    for (std::size_t index = 0; index < kFontCount; ++index) {
        theme.fonts[index].size = kFontSizes[index];
    }
    return theme;
}

Theme Theme::light() {
    Theme theme = dark();
    theme.name = "light";
    for (std::size_t index = 0; index < kColorCount; ++index) {
        theme.colors[index] = math::Color::fromHex(kLightPalette[index]);
    }
    return theme;
}

Theme Theme::fromJson(const core::Json& document, const Theme& base, const TextureLoader& loadTexture) {
    if (!document.is_object()) {
        throw std::invalid_argument("A theme must be a JSON object.");
    }
    core::JsonValidator::requireKnownKeys(document, {"name", "colors", "metrics", "fonts", "fontFiles", "surfaces", "imageFilter"}, "the theme");
    if (!document.contains("name") || !document.at("name").is_string() || document.at("name").get<std::string>().empty()) {
        throw std::invalid_argument("A theme needs a name.");
    }

    Theme theme = base;
    theme.name = document.at("name").get<std::string>();
    if (document.contains("imageFilter")) {
        const core::Json& value = document.at("imageFilter");
        const std::optional<graphics::Texture::Filter> filter = value.is_string() ? graphics::Texture::filterFromName(value.get<std::string>()) : std::nullopt;
        if (!filter) {
            throw std::invalid_argument("The image filter of the theme must be \"nearest\" or \"linear\".");
        }
        theme.imageFilter = *filter;
    }

    for (const auto& [key, value] : readSection(document, "colors").items()) {
        const std::optional<Color> role = colorFromName(key);
        if (!role) {
            throw std::invalid_argument("The theme has no color role named \"" + key + "\".");
        }
        theme.setColor(*role, readColor(value, "The theme color \"" + key + "\""));
    }
    for (const auto& [key, value] : readSection(document, "metrics").items()) {
        const std::optional<Metric> role = metricFromName(key);
        if (!role) {
            throw std::invalid_argument("The theme has no metric named \"" + key + "\".");
        }
        theme.setMetric(*role, readNumber(value, "The theme metric \"" + key + "\""));
    }
    for (const auto& [key, value] : readSection(document, "fontFiles").items()) {
        if (!value.is_string()) {
            throw std::invalid_argument("The theme font file \"" + key + "\" must be a path.");
        }
        theme.fontFiles[key] = value.get<std::string>();
    }
    for (const auto& [key, value] : readSection(document, "fonts").items()) {
        const std::optional<Font> role = fontFromName(key);
        if (!role || !value.is_object()) {
            throw std::invalid_argument("The theme font \"" + key + "\" must be a known role with an object value.");
        }
        core::JsonValidator::requireKnownKeys(value, {"font", "size", "bold", "italic"}, "the theme font \"" + key + "\"");
        FontStyle style = theme.getFont(*role);
        if (value.contains("font")) {
            if (!value.at("font").is_string()) {
                throw std::invalid_argument("The theme font \"" + key + "\" must name its font with a string.");
            }
            style.font = value.at("font").get<std::string>();
        }
        if (value.contains("size")) {
            style.size = readNumber(value.at("size"), "The size of the theme font \"" + key + "\"");
        }
        for (const auto& [flag, field] : {std::pair{"bold", &style.bold}, std::pair{"italic", &style.italic}}) {
            if (!value.contains(flag)) {
                continue;
            }
            if (!value.at(flag).is_boolean()) {
                throw std::invalid_argument("The theme font \"" + key + "\" must set \"" + flag + "\" to \"true\" or \"false\".");
            }
            *field = value.at(flag).get<bool>();
        }
        theme.setFont(*role, std::move(style));
    }
    for (const auto& [key, value] : readSection(document, "surfaces").items()) {
        const std::optional<Surface> role = surfaceFromName(key);
        if (!role) {
            throw std::invalid_argument("The theme has no surface named \"" + key + "\".");
        }
        if (value.is_null()) {
            theme.setSurface(*role, std::nullopt);
            continue;
        }
        if (!loadTexture) {
            throw std::invalid_argument("The theme surface \"" + key + "\" needs a texture loader.");
        }
        theme.setSurface(*role, readImage(value, "the theme surface \"" + key + "\"", loadTexture));
    }
    return theme;
}

const Theme::Image* Theme::getSurface(Surface role) const noexcept {
    const std::optional<Image>& image = surfaces[static_cast<std::size_t>(role)];
    return image ? &*image : nullptr;
}

void Theme::setMetric(Metric role, float value) {
    if (!(value >= 0.0F) || !std::isfinite(value)) {
        throw std::invalid_argument("A theme metric must be a non-negative number.");
    }
    metrics[static_cast<std::size_t>(role)] = value;
}

void Theme::setFont(Font role, FontStyle value) {
    if (value.font.empty() || !(value.size > 0.0F)) {
        throw std::invalid_argument("A theme font needs a font name and a positive size.");
    }
    fonts[static_cast<std::size_t>(role)] = std::move(value);
}

void Theme::setSurface(Surface role, std::optional<Image> value) {
    surfaces[static_cast<std::size_t>(role)] = std::move(value);
}

void Theme::applyTo(ImGuiStyle& style) const {
    style.FontSizeBase = getFont(Font::Body).size;
    style.WindowRounding = getMetric(Metric::ControlRadius);
    style.ChildRounding = getMetric(Metric::ControlRadius);
    style.PopupRounding = getMetric(Metric::ControlRadius);
    style.FrameRounding = getMetric(Metric::ControlRadius) * 0.5F;
    style.GrabRounding = getMetric(Metric::ControlRadius) * 0.5F;
    style.TabRounding = getMetric(Metric::ControlRadius) * 0.5F;
    style.ScrollbarRounding = getMetric(Metric::ScrollbarSize) * 0.5F;
    style.ScrollbarSize = getMetric(Metric::ScrollbarSize);
    style.WindowPadding = {getMetric(Metric::PanelPadding), getMetric(Metric::PanelPadding)};
    style.FramePadding = {getMetric(Metric::ControlPaddingX) * 0.5F, getMetric(Metric::ControlPaddingY) * 0.5F};
    style.ItemSpacing = {getMetric(Metric::ItemSpacing), getMetric(Metric::ItemSpacing) * 0.5F};
    style.WindowBorderSize = getMetric(Metric::BorderWidth) * 0.5F;
    style.FrameBorderSize = 0.0F;
    style.InputTextCursorSize = getMetric(Metric::CaretWidth);

    ImVec4* palette = style.Colors;
    palette[ImGuiCol_Text] = ImGuiConverter::toImVec4(getColor(Color::Text));
    palette[ImGuiCol_TextDisabled] = ImGuiConverter::toImVec4(getColor(Color::TextDisabled));
    palette[ImGuiCol_WindowBg] = ImGuiConverter::toImVec4(getColor(Color::Window));
    palette[ImGuiCol_ChildBg] = ImGuiConverter::toImVec4(math::Color::transparent());
    palette[ImGuiCol_PopupBg] = ImGuiConverter::toImVec4(getColor(Color::Raised));
    palette[ImGuiCol_Border] = ImGuiConverter::toImVec4(getColor(Color::Border));
    palette[ImGuiCol_FrameBg] = ImGuiConverter::toImVec4(getColor(Color::Raised));
    palette[ImGuiCol_FrameBgHovered] = ImGuiConverter::toImVec4(getColor(Color::BorderStrong));
    palette[ImGuiCol_FrameBgActive] = ImGuiConverter::toImVec4(getColor(Color::AccentBackground));
    palette[ImGuiCol_TitleBg] = ImGuiConverter::toImVec4(getColor(Color::Panel));
    palette[ImGuiCol_TitleBgActive] = ImGuiConverter::toImVec4(getColor(Color::Raised));
    palette[ImGuiCol_TitleBgCollapsed] = ImGuiConverter::toImVec4(getColor(Color::Panel));
    palette[ImGuiCol_MenuBarBg] = ImGuiConverter::toImVec4(getColor(Color::Panel));
    palette[ImGuiCol_ScrollbarBg] = ImGuiConverter::toImVec4(math::Color::transparent());
    palette[ImGuiCol_ScrollbarGrab] = ImGuiConverter::toImVec4(getColor(Color::Scrollbar));
    palette[ImGuiCol_ScrollbarGrabHovered] = ImGuiConverter::toImVec4(getColor(Color::ScrollbarHover));
    palette[ImGuiCol_ScrollbarGrabActive] = ImGuiConverter::toImVec4(getColor(Color::ScrollbarHover));
    palette[ImGuiCol_CheckMark] = ImGuiConverter::toImVec4(getColor(Color::Accent));
    palette[ImGuiCol_SliderGrab] = ImGuiConverter::toImVec4(getColor(Color::Accent));
    palette[ImGuiCol_SliderGrabActive] = ImGuiConverter::toImVec4(getColor(Color::AccentStrong));
    palette[ImGuiCol_Button] = ImGuiConverter::toImVec4(getColor(Color::Raised));
    palette[ImGuiCol_ButtonHovered] = ImGuiConverter::toImVec4(getColor(Color::BorderStrong));
    palette[ImGuiCol_ButtonActive] = ImGuiConverter::toImVec4(getColor(Color::Accent));
    palette[ImGuiCol_Header] = ImGuiConverter::toImVec4(getColor(Color::Selection));
    palette[ImGuiCol_HeaderHovered] = ImGuiConverter::toImVec4(getColor(Color::AccentBackground));
    palette[ImGuiCol_HeaderActive] = ImGuiConverter::toImVec4(getColor(Color::Selection));
    palette[ImGuiCol_Separator] = ImGuiConverter::toImVec4(getColor(Color::Border));
    palette[ImGuiCol_SeparatorHovered] = ImGuiConverter::toImVec4(getColor(Color::Accent));
    palette[ImGuiCol_SeparatorActive] = ImGuiConverter::toImVec4(getColor(Color::AccentStrong));
    palette[ImGuiCol_ResizeGrip] = ImGuiConverter::toImVec4(getColor(Color::Border));
    palette[ImGuiCol_ResizeGripHovered] = ImGuiConverter::toImVec4(getColor(Color::Accent));
    palette[ImGuiCol_ResizeGripActive] = ImGuiConverter::toImVec4(getColor(Color::AccentStrong));
    palette[ImGuiCol_Tab] = ImGuiConverter::toImVec4(getColor(Color::Panel));
    palette[ImGuiCol_TabHovered] = ImGuiConverter::toImVec4(getColor(Color::AccentBackground));
    palette[ImGuiCol_TabSelected] = ImGuiConverter::toImVec4(getColor(Color::Raised));
    palette[ImGuiCol_TabSelectedOverline] = ImGuiConverter::toImVec4(getColor(Color::Accent));
    palette[ImGuiCol_PlotLines] = ImGuiConverter::toImVec4(getColor(Color::Accent));
    palette[ImGuiCol_PlotHistogram] = ImGuiConverter::toImVec4(getColor(Color::Accent));
    palette[ImGuiCol_TableHeaderBg] = ImGuiConverter::toImVec4(getColor(Color::Raised));
    palette[ImGuiCol_TableBorderStrong] = ImGuiConverter::toImVec4(getColor(Color::BorderStrong));
    palette[ImGuiCol_TableBorderLight] = ImGuiConverter::toImVec4(getColor(Color::Border));
    palette[ImGuiCol_TableRowBgAlt] = ImGuiConverter::toImVec4(getColor(Color::Hover));
    palette[ImGuiCol_TextSelectedBg] = ImGuiConverter::toImVec4(getColor(Color::Selection));
    palette[ImGuiCol_NavCursor] = ImGuiConverter::toImVec4(getColor(Color::Focus));
    palette[ImGuiCol_ModalWindowDimBg] = ImGuiConverter::toImVec4(getColor(Color::Overlay));
}

} // namespace haylen::ui
