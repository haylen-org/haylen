#include "haylen/plugins/TextPlugin.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "core/EmbeddedFiles.hpp"
#include "graphics/TextureResource.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/JsonNumber.hpp"
#include "haylen/core/JsonValidator.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/io/Path.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/plugins/AssetsPlugin.hpp"
#include "haylen/text/FontFamily.hpp"
#include "haylen/text/TrueTypeFont.hpp"

namespace haylen::plugins {

struct TextPlugin::DecodedFont {
    text::BitmapFont::Description description;
    std::vector<std::string> paths;
    std::vector<graphics::Image> images;
};

// Grid options travel as `{"characters": "ABC", "cellWidth": 8, "cellHeight": 8, "spacing": [1, 1], "margin": [0, 0], "advance": 0, "lineHeight": 0, "baseline": 0}` with the texture options of the image.
text::BitmapFont::Grid TextPlugin::readGrid(const core::Json& options) {
    if (!options.contains("characters") || !options.at("characters").is_string() || !options.contains("cellWidth") || !options.at("cellWidth").is_number() || !options.contains("cellHeight") || !options.at("cellHeight").is_number()) {
        throw std::invalid_argument("A grid font needs its characters as a string and a \"cellWidth\" and \"cellHeight\".");
    }

    // clang-format off
    const auto number = [&options](const char* key) {
        if (!options.contains(key)) {
            return 0.0F;
        }
        if (!options.at(key).is_number()) {
            throw std::invalid_argument(std::string("The grid font option \"") + key + "\" must be a number.");
        }
        return options.at(key).get<float>();
    };
    const auto pair = [&options](const char* key) {
        if (!options.contains(key)) {
            return math::Vec2{};
        }
        const core::Json& value = options.at(key);
        if (!value.is_array() || value.size() != 2 || !value[0].is_number() || !value[1].is_number()) {
            throw std::invalid_argument(std::string("The grid font option \"") + key + "\" must be a pair of numbers.");
        }
        return math::Vec2{value[0].get<float>(), value[1].get<float>()};
    };
    // clang-format on

    return {
        .characters = options.at("characters").get<std::string>(),
        .cellWidth = number("cellWidth"),
        .cellHeight = number("cellHeight"),
        .spacing = pair("spacing"),
        .margin = pair("margin"),
        .advance = number("advance"),
        .lineHeight = number("lineHeight"),
        .baseline = number("baseline"),
    };
}

core::Json TextPlugin::normalizeGrid(const core::Json& options) {
    core::JsonValidator::requireKnownOptions(options, {"characters", "cellWidth", "cellHeight", "spacing", "margin", "advance", "lineHeight", "baseline", "filter", "wrap"}, "grid font options");
    const text::BitmapFont::Grid grid = readGrid(options);
    core::Json texture = core::Json::object();
    for (const char* key : {"filter", "wrap"}) {
        if (options.contains(key)) {
            texture[key] = options.at(key);
        }
    }
    core::Json normalized = assets::Manager::textureOptionsToJson(assets::Manager::textureOptionsFromJson(texture));
    normalized.update({{"characters", grid.characters}, {"cellWidth", core::JsonNumber::fromFloat(grid.cellWidth)}, {"cellHeight", core::JsonNumber::fromFloat(grid.cellHeight)}, {"spacing", {core::JsonNumber::fromFloat(grid.spacing.x), core::JsonNumber::fromFloat(grid.spacing.y)}}, {"margin", {core::JsonNumber::fromFloat(grid.margin.x), core::JsonNumber::fromFloat(grid.margin.y)}}, {"advance", core::JsonNumber::fromFloat(grid.advance)}, {"lineHeight", core::JsonNumber::fromFloat(grid.lineHeight)}, {"baseline", core::JsonNumber::fromFloat(grid.baseline)}});
    return normalized;
}

// The default family draws with the default font of the engine and draws mono text, such as the `[code]` of rich text, with the monospace font the engine embeds.
void TextPlugin::start(core::Engine& engine) {
    const std::span<const std::uint8_t> monospace = core::EmbeddedFiles::getMonospaceFont();
    std::shared_ptr<text::Font> mono = std::make_shared<text::TrueTypeFont>(engine.getGraphics(), std::vector<std::uint8_t>(monospace.begin(), monospace.end()));
    registry = std::make_shared<text::RichTextRegistry>(std::make_shared<text::FontFamily>(text::FontFamily::Faces{.regular = engine.getDefaultFont(), .mono = std::move(mono)}));
    assets::Manager& manager = engine.getAssets();
    graphics::Device& device = engine.getGraphics();

    // Page images of a BMFont sit next to the file and load like textures with the options of the font, so they share the texture cache.
    // clang-format off
    manager.registerType({
        .name = "bitmapFont",
        .extensions = {".fnt"},
        .normalize = [&manager](const core::Json& options) { return manager.normalizeOptions("texture", options); },
        .decode = [](assets::Manager::Request& request) -> std::shared_ptr<void> {
            auto decoded = std::make_shared<DecodedFont>();
            decoded->description = text::BitmapFont::parse(request.bytes);
            for (const std::string& page : decoded->description.pages) {
                decoded->paths.push_back(io::Path::join(io::Path::directory(request.path), page));
                decoded->images.push_back(graphics::Image::decode(request.package->readAsset(decoded->paths.back())));
            }
            return decoded;
        },
        .finalize = [&manager, &device](std::shared_ptr<void> result, const assets::Manager::Request& request) -> std::shared_ptr<void> {
            const auto& decoded = *std::static_pointer_cast<DecodedFont>(result);
            std::vector<graphics::Texture> pages;
            for (std::size_t index = 0; index < decoded.paths.size(); ++index) {
                const auto resource = manager.share("texture", decoded.paths[index], request.options, [&](const assets::Manager::Request& texture) {
                    return device.createTexture(decoded.images[index], assets::Manager::textureOptionsFromJson(texture.options)).getResource();
                });
                pages.emplace_back(std::static_pointer_cast<graphics::TextureResource>(resource));
            }
            const std::shared_ptr<text::Font> font = std::make_shared<text::BitmapFont>(decoded.description, std::move(pages));
            return font;
        },
    });
    manager.registerType({
        .name = "gridFont",
        .extensions = {},
        .normalize = &normalizeGrid,
        .decode = [](assets::Manager::Request& request) -> std::shared_ptr<void> {
            return std::make_shared<graphics::Image>(graphics::Image::decode(request.bytes));
        },
        .finalize = [&manager, &device](std::shared_ptr<void> result, const assets::Manager::Request& request) -> std::shared_ptr<void> {
            const graphics::Image& image = *std::static_pointer_cast<graphics::Image>(result);
            const core::Json textureOptions{{"filter", request.options.at("filter")}, {"wrap", request.options.at("wrap")}};
            const auto resource = manager.share("texture", request.path, textureOptions, [&](const assets::Manager::Request& texture) {
                return device.createTexture(image, assets::Manager::textureOptionsFromJson(texture.options)).getResource();
            });
            const graphics::Texture page(std::static_pointer_cast<graphics::TextureResource>(resource));
            const std::shared_ptr<text::Font> font = std::make_shared<text::BitmapFont>(text::BitmapFont::describeGrid(readGrid(request.options), page.getSize()), std::vector<graphics::Texture>{page});
            return font;
        },
    });
    for (const char* type : {"bitmapFont", "gridFont"}) {
        engine.getPlugin<AssetsPlugin>().registerLuaPusher(type, [](lua_State* L, const std::shared_ptr<void>& asset) {
            lua::Stack::push(L, std::static_pointer_cast<text::Font>(asset));
        });
    }
    // clang-format on
}

// Effects registered from Lua hold Lua functions, which must go before the Lua state closes.
void TextPlugin::stop(core::Engine&) {
    drawnTexts.clear();
    registry->clear();
    images.clear();
}

void TextPlugin::endFrame(core::Engine&) {
    std::erase_if(images, [](const auto& entry) { return !entry.second.used; });
    for (auto& [path, image] : images) {
        image.used = false;
    }
    std::erase_if(drawnTexts, [](const auto& entry) { return !entry.second.used; });
    for (auto& [hash, drawn] : drawnTexts) {
        drawn.used = false;
    }
}

std::size_t TextPlugin::hashDrawn(std::string_view markup, const text::RichTextOptions& options) noexcept {
    std::size_t hash = std::hash<std::string_view>{}(markup);
    for (const std::size_t part : {std::hash<const void*>{}(options.family.get()), std::hash<float>{}(options.size), std::hash<float>{}(options.maxWidth), std::hash<float>{}(options.scale), std::hash<std::string_view>{}(options.language)}) {
        hash ^= part + std::size_t{0x9E3779B9U} + (hash << 6U) + (hash >> 2U);
    }
    return hash;
}

bool TextPlugin::isSameDrawn(const DrawnText& drawn, std::string_view markup, const text::RichTextOptions& options, std::span<const FontName> fonts) noexcept {
    const text::RichTextOptions& kept = drawn.options;
    const bool sameOptions = kept.family == options.family && kept.size == options.size && kept.bold == options.bold && kept.italic == options.italic && kept.color == options.color && kept.maxWidth == options.maxWidth && kept.align == options.align && kept.direction == options.direction && kept.language == options.language && kept.lineSpacing == options.lineSpacing && kept.scale == options.scale && kept.revealSpeed == options.revealSpeed && kept.underlineLinks == options.underlineLinks;
    return sameOptions && drawn.markup == markup && std::ranges::equal(drawn.fonts, fonts);
}

text::RichText& TextPlugin::getDrawnText(std::string_view markup, const text::RichTextOptions& options, std::span<const FontName> fonts, double seconds) {
    const std::size_t hash = hashDrawn(markup, options);
    auto [first, last] = drawnTexts.equal_range(hash);
    auto found = std::find_if(first, last, [&](const auto& entry) { return isSameDrawn(entry.second, markup, options, fonts); });
    if (found == last) {
        auto made = std::make_unique<text::RichText>(std::string(markup), options, getRegistry());
        found = drawnTexts.emplace(hash, DrawnText{.markup = std::string(markup), .options = options, .fonts = {fonts.begin(), fonts.end()}, .text = std::move(made)});
    }

    DrawnText& drawn = found->second;
    drawn.used = true;
    const auto time = static_cast<float>(seconds);
    if (time > drawn.text->getTime()) {
        drawn.text->update(time - drawn.text->getTime());
    }
    return *drawn.text;
}

graphics::Texture TextPlugin::getImage(core::Engine& engine, std::string_view path) {
    auto found = images.find(path);
    if (found == images.end()) {
        found = images.emplace(std::string(path), CachedImage{.texture = engine.getAssets().texture(path)}).first;
    }
    found->second.used = true;
    return found->second.texture;
}

const std::shared_ptr<text::RichTextRegistry>& TextPlugin::getRegistry() const {
    if (!registry) {
        throw std::logic_error("The text plugin has not started.");
    }
    return registry;
}

} // namespace haylen::plugins
