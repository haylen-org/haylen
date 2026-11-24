#include "2d/graphics/Graphics2DBinding.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "2d/graphics/CameraBinding.hpp"
#include "2d/graphics/CameraRigBinding.hpp"
#include "2d/graphics/GlassBinding.hpp"
#include "2d/graphics/GroupBinding.hpp"
#include "2d/graphics/MaterialBinding.hpp"
#include "2d/graphics/MeshBinding.hpp"
#include "2d/graphics/NineSliceBinding.hpp"
#include "2d/graphics/ParallaxBinding.hpp"
#include "2d/graphics/PathBinding.hpp"
#include "2d/graphics/PixelCanvasBinding.hpp"
#include "2d/graphics/RichTextBinding.hpp"
#include "2d/graphics/SpriteBatchBinding.hpp"
#include "2d/graphics/SpriteBinding.hpp"
#include "2d/graphics/StrokeBinding.hpp"
#include "2d/graphics/TileLayerBinding.hpp"
#include "2d/lighting/LightBinding.hpp"
#include "2d/lighting/OccluderBinding.hpp"
#include "core/FloatBufferBinding.hpp"
#include "graphics/FontBinding.hpp"
#include "haylen/2d/graphics/ImageBlend.hpp"
#include "haylen/2d/graphics/Parallax.hpp"
#include "haylen/2d/graphics/SpriteBatch.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/math/Geometry2.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/text/Font.hpp"
#include "haylen/text/FontFamily.hpp"
#include "math/PolygonBinding.hpp"

namespace haylen::lua::graphics2d {

haylen::graphics2d::Renderer& Graphics2DBinding::getRenderer(lua_State* L) {
    return lua::Runtime::getEngine(L).getRenderer2D();
}

haylen::text::Font& Graphics2DBinding::fontArgument(lua_State* L, int index) {
    return lua_isnoneornil(L, index) ? *lua::Runtime::getEngine(L).getDefaultFont() : lua::Userdata::check<haylen::text::Font>(L, index);
}

haylen::graphics2d::Renderer::CanvasOptions Graphics2DBinding::readCanvasOptions(lua_State* L, int index) {
    haylen::graphics2d::Renderer::CanvasOptions options;
    if (lua_isnoneornil(L, index)) {
        return options;
    }

    luaL_checktype(L, index, LUA_TTABLE);
    // clang-format off
    lua::Table::readFields(L, index, kCanvasFields, {}, [L, &options](std::string_view key) {
        if (key == "sort") {
            lua::Table::readValue(L, key, options.sort);
        } else if (key == "order") {
            lua::Table::readValue(L, key, options.order);
        } else if (key == "visibilityMask") {
            lua::Table::readValue(L, key, options.visibilityMask);
        } else if (key == "ambientLight") {
            lua::Table::readValue(L, key, options.ambientLight);
        } else if (key == "clear") {
            lua::Table::readValue(L, key, options.clear);
        } else if (key == "debugDrawings") {
            lua::Table::readValue(L, key, options.debugDrawings);
        } else {
            options.postProcess = readPostProcess(L, lua_gettop(L));
        }
    });
    // clang-format on
    return options;
}

haylen::graphics2d::PostProcess Graphics2DBinding::readPostProcess(lua_State* L, int post) {
    luaL_checktype(L, post, LUA_TTABLE);
    haylen::graphics2d::PostProcess process;
    // clang-format off
    lua::Table::readFields(L, post, kPostProcessFields, {}, [L, &process](std::string_view key) {
        if (key == "tint") {
            lua::Table::readValue(L, key, process.tint);
        } else if (key == "saturation") {
            lua::Table::readValue(L, key, process.saturation);
        } else if (key == "brightness") {
            lua::Table::readValue(L, key, process.brightness);
        } else if (key == "contrast") {
            lua::Table::readValue(L, key, process.contrast);
        } else if (key == "vignetteStrength") {
            lua::Table::readValue(L, key, process.vignetteStrength);
        } else if (key == "vignetteRadius") {
            lua::Table::readValue(L, key, process.vignetteRadius);
        } else if (key == "vignetteSoftness") {
            lua::Table::readValue(L, key, process.vignetteSoftness);
        } else if (key == "distortion") {
            lua::Table::readValue(L, key, process.distortion);
        } else if (key == "chromaticAberration") {
            lua::Table::readValue(L, key, process.chromaticAberration);
        } else if (key == "pixelate") {
            lua::Table::readValue(L, key, process.pixelate);
        } else if (key == "blur") {
            lua::Table::readValue(L, key, process.blur);
        } else if (key == "bloomStrength") {
            lua::Table::readValue(L, key, process.bloomStrength);
        } else if (key == "bloomThreshold") {
            lua::Table::readValue(L, key, process.bloomThreshold);
        } else if (key == "bloomRadius") {
            lua::Table::readValue(L, key, process.bloomRadius);
        } else if (key == "colorLutStrength") {
            lua::Table::readValue(L, key, process.colorLutStrength);
        } else if (key == "grain") {
            lua::Table::readValue(L, key, process.grain);
        } else if (key == "fade") {
            lua::Table::readValue(L, key, process.fade);
        } else if (key == "colorLut") {
            lua::Table::readValue(L, key, process.colorLut);
        } else if (key == "colorVision") {
            process.colorVision = readColorVision(L);
        } else if (key == "correctColorVision") {
            lua::Table::readValue(L, key, process.correctColorVision);
        } else if (key == "dither") {
            lua::Table::readValue(L, key, process.dither);
        } else {
            lua::Table::readValue(L, key, process.materials);
        }
    });
    // clang-format on
    return process;
}

haylen::graphics2d::PostProcess::ColorVision Graphics2DBinding::readColorVision(lua_State* L) {
    const auto name = lua::Stack::read<std::string_view>(L, -1);
    const auto found = std::ranges::find(haylen::graphics2d::PostProcess::kColorVisionNames, name, &std::pair<std::string_view, haylen::graphics2d::PostProcess::ColorVision>::first);
    if (found == haylen::graphics2d::PostProcess::kColorVisionNames.end()) {
        luaL_error(L, "The color vision \"%s\" is unknown. It is \"normal\", \"protanopia\", \"deuteranopia\" or \"tritanopia\".", std::string(name).c_str());
    }

    return found->second;
}

void Graphics2DBinding::assignFields(lua_State* L, int options) {
    if (lua_isnoneornil(L, options)) {
        return;
    }

    luaL_checktype(L, options, LUA_TTABLE);
    const int object = lua_gettop(L);
    lua_pushnil(L);
    while (lua_next(L, options) != 0) {
        lua_pushvalue(L, -2);
        lua_insert(L, -2);
        lua_settable(L, object);
    }
}

// Creates a sprite with `newSprite(texture, {x = 10, y = 20, layer = 2, ...})`.
int Graphics2DBinding::newSprite(lua_State* L) {
    haylen::graphics2d::Sprite sprite;
    sprite.texture = lua::Stack::read<haylen::graphics::Texture>(L, 1);

    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
    }

    lua_settop(L, 2);
    lua::Userdata::emplace<haylen::graphics2d::Sprite>(L, std::move(sprite));
    assignFields(L, 2);
    return 1;
}

// Creates a parallax layer with `newParallax(texture, {scrollScale = {0.5, 0.5}, repeatX = true, autoscroll = {-20, 0}, ...})`.
int Graphics2DBinding::newParallax(lua_State* L) {
    haylen::graphics2d::Parallax parallax;
    parallax.texture = lua::Stack::read<haylen::graphics::Texture>(L, 1);

    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
    }

    lua_settop(L, 2);
    lua::Userdata::emplace<haylen::graphics2d::Parallax>(L, std::move(parallax));
    assignFields(L, 2);
    return 1;
}

int Graphics2DBinding::blendCameras(lua_State* L) {
    lua::Userdata::emplace<haylen::graphics2d::Camera>(L, haylen::graphics2d::Camera::blend(lua::Userdata::check<haylen::graphics2d::Camera>(L, 1), lua::Userdata::check<haylen::graphics2d::Camera>(L, 2), lua::Stack::read<float>(L, 3)));
    return 1;
}

// Makes a batch of one texture with `newSpriteBatch(texture)`, or of several with `newSpriteBatch({texture, ...})`.
int Graphics2DBinding::newSpriteBatch(lua_State* L) {
    SpriteBatchBinding::Textures textures;
    const std::span<const haylen::graphics::Texture> read = SpriteBatchBinding::readTextures(L, 1, textures);
    lua::Userdata::emplace<haylen::graphics2d::SpriteBatch>(L, std::make_shared<haylen::graphics2d::SpriteBatch>(std::vector<haylen::graphics::Texture>(read.begin(), read.end())));
    return 1;
}

int Graphics2DBinding::newCamera(lua_State* L) {
    lua::Userdata::emplace<haylen::graphics2d::Camera>(L);
    return 1;
}

// Builds a nine-slice with `newNineSlice(texture, {source = rect, borders = {left, top, right, bottom}})` or `newNineSlice(texture, {pieces = {nine rects}})`.
int Graphics2DBinding::newNineSlice(lua_State* L) {
    const haylen::graphics::Texture texture = lua::Stack::read<haylen::graphics::Texture>(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    lua::Table::checkFields(L, 2, {kNineSliceFields});

    haylen::graphics2d::NineSlice slice;
    lua_getfield(L, 2, "pieces");
    if (!lua_isnil(L, -1)) {
        const std::vector<haylen::math::Rect> pieces = lua::Stack::read<std::vector<haylen::math::Rect>>(L, -1);
        luaL_argcheck(L, pieces.size() == 9, 2, "a nine-slice needs exactly nine pieces");
        std::array<haylen::math::Rect, 9> regions{};
        std::copy(pieces.begin(), pieces.end(), regions.begin());
        slice = haylen::graphics2d::NineSlice::fromPieces(texture, regions);
    } else {
        haylen::math::Rect source{};
        std::array<float, 4> borders{};
        lua::Table::readField(L, 2, "source", source);
        lua_getfield(L, 2, "borders");
        const std::vector<float> values = lua::Stack::read<std::vector<float>>(L, -1);
        luaL_argcheck(L, values.size() == 4, 2, "borders need left, top, right and bottom");
        std::copy(values.begin(), values.end(), borders.begin());
        lua_pop(L, 1);
        slice = haylen::graphics2d::NineSlice::fromBorders(texture, source, {borders[0], borders[1], borders[2], borders[3]});
    }

    lua_pop(L, 1);

    lua::Table::readField(L, 2, "fill", slice.fill);
    lua::Userdata::emplace<haylen::graphics2d::NineSlice>(L, std::move(slice));
    return 1;
}

int Graphics2DBinding::beginWorld(lua_State* L) {
    getRenderer(L).beginWorld(lua::Userdata::check<haylen::graphics2d::Camera>(L, 1), readCanvasOptions(L, 2));
    return 0;
}

int Graphics2DBinding::beginScreen(lua_State* L) {
    getRenderer(L).beginScreen(readCanvasOptions(L, 1));
    return 0;
}

int Graphics2DBinding::beginTarget(lua_State* L) {
    getRenderer(L).beginTarget(lua::Userdata::check<haylen::graphics::RenderTarget>(L, 1), lua::Userdata::check<haylen::graphics2d::Camera>(L, 2), readCanvasOptions(L, 3));
    return 0;
}

// Captures the following canvases with `beginCapture(target, options)`, whose `clear` color is transparent unless given.
int Graphics2DBinding::beginCapture(lua_State* L) {
    haylen::math::Color clear = haylen::math::Color::transparent();
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kCaptureFields});
        lua::Table::readField(L, 2, "clear", clear);
    }

    getRenderer(L).beginCapture(lua::Userdata::check<haylen::graphics::RenderTarget>(L, 1), clear);
    return 0;
}

int Graphics2DBinding::endCapture(lua_State* L) {
    getRenderer(L).endCapture();
    return 0;
}

// Draws a texture immediately with `draw(texture, x, y, {source, width, height, scaleX, scaleY, pivotX, pivotY, rotation, tilt, color, flash, flipHorizontal, flipVertical, flipDiagonal, layer, depth, blend})`.
int Graphics2DBinding::draw(lua_State* L) {
    haylen::graphics2d::Sprite sprite;
    sprite.texture = lua::Stack::read<haylen::graphics::Texture>(L, 1);
    sprite.position = {lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)};
    if (!lua_isnoneornil(L, 4)) {
        const haylen::graphics2d::SpriteInstance instance = lua::TypeConverter::readSpriteInstance(L, 4, sprite.texture, {.position = sprite.position}, {kDrawFields, lua::TypeConverter::kDrawOrderFields});
        sprite.position = instance.position;
        sprite.source = instance.source;
        sprite.size = instance.size;
        sprite.pivot = instance.pivot;
        sprite.rotation = instance.rotation;
        sprite.tilt = instance.tilt;
        sprite.color = instance.color;
        sprite.flash = instance.flash;
        sprite.flip = instance.flip;
        // clang-format off
        lua::Table::readFields(L, 4, kDrawFields, {lua::TypeConverter::kSpriteInstanceFields, lua::TypeConverter::kDrawOrderFields}, [L, &sprite](std::string_view key) {
            if (key == "scaleX") {
                lua::Table::readValue(L, key, sprite.scale.x);
            } else if (key == "scaleY") {
                lua::Table::readValue(L, key, sprite.scale.y);
            } else if (key == "partColors") {
                lua::Table::readValue(L, key, sprite.partColors);
            } else if (key == "effect") {
                lua::Table::readValue(L, key, sprite.effect);
            }
        });
        // clang-format on
        sprite.order = lua::TypeConverter::readDrawOrder(L, 4, {lua::TypeConverter::kSpriteInstanceFields, kDrawFields});
    }

    getRenderer(L).draw(sprite);
    return 0;
}

// Draws a vector image with `drawVector(image, x, y[, options])`, at the size of the image unless the options give one, where the options take the keys of `draw` but `source` and `partColors`.
int Graphics2DBinding::drawVector(lua_State* L) {
    const haylen::graphics::VectorImage& image = lua::Userdata::check<haylen::graphics::VectorImage>(L, 1);
    haylen::graphics2d::Sprite sprite;
    sprite.position = {lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)};
    if (!lua_isnoneornil(L, 4)) {
        luaL_checktype(L, 4, LUA_TTABLE);
        // clang-format off
        lua::Table::readFields(L, 4, kVectorFields, {lua::TypeConverter::kDrawOrderFields}, [L, &sprite](std::string_view key) {
            if (key == "x") {
                lua::Table::readValue(L, key, sprite.position.x);
            } else if (key == "y") {
                lua::Table::readValue(L, key, sprite.position.y);
            } else if (key == "width") {
                lua::Table::readValue(L, key, sprite.size.x);
            } else if (key == "height") {
                lua::Table::readValue(L, key, sprite.size.y);
            } else if (key == "scaleX") {
                lua::Table::readValue(L, key, sprite.scale.x);
            } else if (key == "scaleY") {
                lua::Table::readValue(L, key, sprite.scale.y);
            } else if (key == "pivotX") {
                lua::Table::readValue(L, key, sprite.pivot.x);
            } else if (key == "pivotY") {
                lua::Table::readValue(L, key, sprite.pivot.y);
            } else if (key == "rotation") {
                lua::Table::readValue(L, key, sprite.rotation);
            } else if (key == "tilt") {
                lua::Table::readValue(L, key, sprite.tilt);
            } else if (key == "color") {
                lua::Table::readValue(L, key, sprite.color);
            } else if (key == "flash") {
                lua::Table::readValue(L, key, sprite.flash);
            } else if (key == "flipHorizontal") {
                lua::Table::readValue(L, key, sprite.flip.horizontal);
            } else if (key == "flipVertical") {
                lua::Table::readValue(L, key, sprite.flip.vertical);
            } else if (key == "flipDiagonal") {
                lua::Table::readValue(L, key, sprite.flip.diagonal);
            }
        });
        // clang-format on
        sprite.order = lua::TypeConverter::readDrawOrder(L, 4, {kVectorFields});
    }

    getRenderer(L).drawVector(image, sprite);
    return 0;
}

// Draws a list of sprite tables of one texture, or of a list of textures that each sprite picks from with its `texture`, as a single batch with `drawBatch(textures, {{x, y, width, height, ...}, ...}, order)`, or the sprites a float buffer holds with `drawBatch(textures, buffer, {fields = {'x', 'y', ...}, width = ..., ...}, order)`, where the table is the template of every sprite.
int Graphics2DBinding::drawBatch(lua_State* L) {
    SpriteBatchBinding::Textures held;
    const std::span<const haylen::graphics::Texture> textures = SpriteBatchBinding::readTextures(L, 1, held);
    haylen::graphics2d::SpriteBatch::checkTextures(textures);

    if (const haylen::core::FloatBuffer* buffer = lua::Userdata::test<haylen::core::FloatBuffer>(L, 2)) {
        return drawBufferBatch(L, textures, *buffer);
    }

    luaL_checktype(L, 2, LUA_TTABLE);
    std::vector<haylen::graphics2d::SpriteInstance>& sprites = batchSprites;
    std::vector<haylen::graphics2d::PartColors>& partColors = batchPartColors;
    std::vector<haylen::graphics2d::SpriteData>& data = batchData;
    sprites.resize(static_cast<std::size_t>(lua_rawlen(L, 2)));
    partColors.clear();
    data.clear();
    for (std::size_t index = 0; index < sprites.size(); ++index) {
        lua_rawgeti(L, 2, static_cast<lua_Integer>(index + 1));
        lua::Table::requireItemType(L, "sprites", static_cast<lua_Integer>(index + 1), LUA_TTABLE);
        const std::optional<haylen::graphics2d::SpriteData> spriteData = SpriteBatchBinding::readSpriteData(L, -1, textures.size());
        if (spriteData) {
            data.resize(sprites.size());
            data[index] = *spriteData;
        }

        sprites[index] = lua::TypeConverter::readSpriteInstance(L, -1, textures[spriteData ? spriteData->texture : 0], {}, {SpriteBatchBinding::kPartColorFields, SpriteBatchBinding::kDataFields});
        if (lua_getfield(L, -1, "partColors") != LUA_TNIL) {
            partColors.resize(sprites.size());
            partColors[index] = lua::Stack::read<haylen::graphics2d::PartColors>(L, -1);
        }

        lua_pop(L, 2);
    }

    getRenderer(L).drawBatch(textures, sprites, data, lua::TypeConverter::readDrawOrder(L, 3), partColors);
    return 0;
}

// The table at 3 is the template of every sprite, with the fields the buffer holds for each sprite.
int Graphics2DBinding::drawBufferBatch(lua_State* L, std::span<const haylen::graphics::Texture> textures, const haylen::core::FloatBuffer& buffer) {
    luaL_checktype(L, 3, LUA_TTABLE);
    const haylen::graphics2d::SpriteData data = SpriteBatchBinding::readSpriteData(L, 3, textures.size()).value_or(haylen::graphics2d::SpriteData{});
    const haylen::graphics2d::SpriteInstance sprite = lua::TypeConverter::readSpriteInstance(L, 3, textures[data.texture], {}, {SpriteBatchBinding::kLayoutFields, SpriteBatchBinding::kDataFields});
    if (lua_getfield(L, 3, "fields") != LUA_TTABLE) {
        return luaL_error(L, "A \"drawBatch\" call with a float buffer needs the fields each sprite takes, such as \"fields = {'x', 'y'}\".");
    }

    const haylen::graphics2d::SpriteLayout layout = SpriteBatchBinding::readLayout(L, -1, sprite, data);
    lua_pop(L, 1);
    getRenderer(L).drawBatch(textures, buffer.getValues(), layout, lua::TypeConverter::readDrawOrder(L, 4));
    return 0;
}

// Draws a baked batch with `drawStatic(batch[, x, y[, order]])`, where `x` and `y` shift the whole batch without rebaking it.
int Graphics2DBinding::drawStatic(lua_State* L) {
    const haylen::math::Vec2 offset{lua::Converter<float>::readOr(L, 2, 0.0), lua::Converter<float>::readOr(L, 3, 0.0)};
    getRenderer(L).drawStatic(lua::Userdata::check<haylen::graphics2d::StaticSpriteBatch>(L, 1), lua::TypeConverter::readDrawOrder(L, 4), offset);
    return 0;
}

int Graphics2DBinding::drawRect(lua_State* L) {
    getRenderer(L).drawRect(lua::Stack::read<haylen::math::Rect>(L, 1), lua::Stack::read<haylen::math::Color>(L, 2), lua::TypeConverter::readDrawOrder(L, 3));
    return 0;
}

int Graphics2DBinding::drawRectOutline(lua_State* L) {
    getRenderer(L).drawRectOutline(lua::Stack::read<haylen::math::Rect>(L, 1), lua::Stack::read<float>(L, 2), lua::Stack::read<haylen::math::Color>(L, 3), lua::TypeConverter::readDrawOrder(L, 4));
    return 0;
}

int Graphics2DBinding::drawLine(lua_State* L) {
    getRenderer(L).drawLine({lua::Stack::read<float>(L, 1), lua::Stack::read<float>(L, 2)}, {lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)}, lua::Stack::read<float>(L, 5), lua::Stack::read<haylen::math::Color>(L, 6), lua::TypeConverter::readDrawOrder(L, 7));
    return 0;
}

int Graphics2DBinding::drawCircle(lua_State* L) {
    getRenderer(L).drawCircle({lua::Stack::read<float>(L, 1), lua::Stack::read<float>(L, 2)}, lua::Stack::read<float>(L, 3), lua::Stack::read<haylen::math::Color>(L, 4), lua::TypeConverter::readDrawOrder(L, 5));
    return 0;
}

int Graphics2DBinding::drawEllipse(lua_State* L) {
    getRenderer(L).drawEllipse({lua::Stack::read<float>(L, 1), lua::Stack::read<float>(L, 2)}, {lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)}, lua::Stack::read<haylen::math::Color>(L, 5), lua::TypeConverter::readDrawOrder(L, 6));
    return 0;
}

// Draws with `drawPoints(points, {radius, color, colors, count, layer, ...})`, whose points come from a flat list of numbers or a float buffer.
int Graphics2DBinding::drawPoints(lua_State* L) {
    StrokeBinding::readPoints(L, 1, pointScratch);
    float radius = 1.0F;
    std::optional<std::size_t> count;
    std::optional<haylen::math::Color> color;
    bool eachColor = false;
    colorScratch.clear();
    haylen::graphics2d::DrawOrder order;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        order = lua::TypeConverter::readDrawOrder(L, 2, {kPointFields});
        // clang-format off
        lua::Table::readFields(L, 2, kPointFields, {lua::TypeConverter::kDrawOrderFields}, [L, &radius, &count, &color, &eachColor](std::string_view key) {
            if (key == "radius") {
                lua::Table::readValue(L, key, radius);
            } else if (key == "color") {
                lua::Table::readValue(L, key, color.emplace());
            } else if (key == "colors") {
                lua::Table::readValue(L, key, colorScratch);
                eachColor = true;
            } else {
                lua::Table::readValue(L, key, count.emplace());
            }
        });
        // clang-format on
    }

    if (color && eachColor) {
        throw std::invalid_argument("Points take either one \"color\" or \"colors\" for each point.");
    }

    if (eachColor && colorScratch.size() != pointScratch.size()) {
        throw std::invalid_argument(std::format("Points take one color for each of their {} points.", pointScratch.size()));
    }

    if (!eachColor) {
        colorScratch.push_back(color.value_or(haylen::math::Color::white()));
    }

    const std::size_t drawn = count.value_or(pointScratch.size());
    if (drawn > pointScratch.size()) {
        throw std::invalid_argument(std::format("The points draw {} of the {} they were given.", drawn, pointScratch.size()));
    }

    const std::span<const haylen::math::Color> used = eachColor ? std::span<const haylen::math::Color>(colorScratch).first(drawn) : std::span<const haylen::math::Color>(colorScratch);
    getRenderer(L).drawPoints(std::span(pointScratch).first(drawn), radius, used, order);
    return 0;
}

int Graphics2DBinding::drawRing(lua_State* L) {
    getRenderer(L).drawRing({lua::Stack::read<float>(L, 1), lua::Stack::read<float>(L, 2)}, lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4), lua::Stack::read<haylen::math::Color>(L, 5), lua::TypeConverter::readDrawOrder(L, 6));
    return 0;
}

int Graphics2DBinding::drawArc(lua_State* L) {
    getRenderer(L).drawArc({lua::Stack::read<float>(L, 1), lua::Stack::read<float>(L, 2)}, lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4), lua::Stack::read<float>(L, 5), lua::Stack::read<float>(L, 6), lua::Stack::read<haylen::math::Color>(L, 7), lua::TypeConverter::readDrawOrder(L, 8));
    return 0;
}

// Reads the value on top of the stack as one radius for every corner or as four from the top-left corner clockwise.
void Graphics2DBinding::readRadii(lua_State* L, std::array<float, 4>& radii) {
    if (lua_type(L, -1) == LUA_TNUMBER) {
        radii.fill(lua::Converter<float>::fromNumber(L, lua_tonumber(L, -1)));
        return;
    }

    if (lua_type(L, -1) != LUA_TTABLE || lua_rawlen(L, -1) != radii.size()) {
        throw std::invalid_argument("The option \"radius\" of a shape takes one radius for every corner or four, from the top-left corner clockwise.");
    }

    for (std::size_t corner = 0; corner < radii.size(); ++corner) {
        lua_rawgeti(L, -1, static_cast<lua_Integer>(corner + 1));
        lua::Table::readValue(L, "radius", radii[corner]);
        lua_pop(L, 1);
    }
}

void Graphics2DBinding::readShapeField(lua_State* L, std::string_view key, haylen::graphics2d::Shape& shape) {
    if (key == "kind") {
        lua::Table::readValue(L, key, shape.kind);
    } else if (key == "radius") {
        readRadii(L, shape.radii);
    } else if (key == "rotation") {
        lua::Table::readValue(L, key, shape.rotation);
    } else if (key == "startAngle") {
        lua::Table::readValue(L, key, shape.startAngle);
    } else if (key == "sweep") {
        lua::Table::readValue(L, key, shape.sweep);
    } else if (key == "color") {
        lua::Table::readValue(L, key, shape.color);
    } else if (key == "texture") {
        lua::Table::readValue(L, key, shape.texture);
    } else if (key == "borderWidth") {
        lua::Table::readValue(L, key, shape.borderWidth);
    } else if (key == "borderColor") {
        lua::Table::readValue(L, key, shape.borderColor);
    } else if (key == "softness") {
        lua::Table::readValue(L, key, shape.softness);
    }
}

// Draws a rounded rectangle, an ellipse, a circle, a capsule, a ring or an arc with `drawShape(rect, {kind, radius, rotation, startAngle, sweep, color, texture, borderWidth, borderColor, softness, layer, depth, ...})`.
int Graphics2DBinding::drawShape(lua_State* L) {
    haylen::graphics2d::Shape shape{.bounds = lua::Stack::read<haylen::math::Rect>(L, 1)};
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        // clang-format off
        lua::Table::readFields(L, 2, kShapeFields, {lua::TypeConverter::kDrawOrderFields}, [L, &shape](std::string_view key) {
            readShapeField(L, key, shape);
        });
        // clang-format on
    }

    getRenderer(L).drawShape(shape, lua::TypeConverter::readDrawOrder(L, 2, {kShapeFields}));
    return 0;
}

// Fills a polygon or a shape of outlines with `drawPolygon(points or shape, color, order)`, where a shape takes the outlines of `m.polygon`.
int Graphics2DBinding::drawPolygon(lua_State* L) {
    lua::math::PolygonBinding::readShape(L, 1, pointScratch, countScratch);
    getRenderer(L).drawPolygon(pointScratch, countScratch, lua::Stack::read<haylen::math::Color>(L, 2), lua::TypeConverter::readDrawOrder(L, 3));
    return 0;
}

int Graphics2DBinding::drawPolyline(lua_State* L) {
    lua::Stack::readInto(L, 1, pointScratch);
    getRenderer(L).drawPolyline(pointScratch, lua::Stack::read<float>(L, 2), lua::Stack::read<haylen::math::Color>(L, 3), lua_toboolean(L, 4) != 0, lua::TypeConverter::readDrawOrder(L, 5));
    return 0;
}

// Draws triangles with `drawMesh(texture or nil, {{x, y, u, v, color}, ...}, {1, 2, 3, ...}, options)`, whose options take `fadeEdges` and the keys of the draw order.
int Graphics2DBinding::drawMesh(lua_State* L) {
    const haylen::graphics::Texture texture = lua_isnil(L, 1) ? haylen::graphics::Texture{} : lua::Stack::read<haylen::graphics::Texture>(L, 1);
    std::vector<haylen::graphics2d::MeshVertex>& vertices = meshVertices;
    std::vector<std::uint32_t>& indices = meshIndices;
    MeshBinding::readVertices(L, 2, vertices);
    MeshBinding::readIndices(L, 3, indices);
    bool fadeEdges = true;
    if (lua_istable(L, 4)) {
        lua::Table::readField(L, 4, "fadeEdges", fadeEdges);
    }

    getRenderer(L).drawMesh(texture, vertices, indices, lua::TypeConverter::readDrawOrder(L, 4, {kMeshFields}), fadeEdges);
    return 0;
}

// Draws text with `drawText(font, text, x, y, style)`, where the font is a `Font`, a `FontFamily` or `nil` for the default font, and the style also takes the keys of the draw order. A family draws what its faces lack from its fallbacks.
int Graphics2DBinding::drawText(lua_State* L) {
    const std::string_view content = lua::Stack::read<std::string_view>(L, 2);
    const haylen::math::Vec2 position{lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)};
    const haylen::text::Style style = lua::TypeConverter::readTextStyle(L, 5, {lua::TypeConverter::kDrawOrderFields});
    const haylen::graphics2d::DrawOrder order = lua::TypeConverter::readDrawOrder(L, 5, {lua::TypeConverter::kTextStyleFields});
    if (haylen::text::FontFamily* family = lua::Userdata::test<haylen::text::FontFamily>(L, 1)) {
        getRenderer(L).drawText(*family, content, position, style, order);
    } else {
        getRenderer(L).drawText(fontArgument(L, 1), content, position, style, order);
    }

    return 0;
}

// Measures text with the same font or family and style table `drawText` takes, whose draw order keys change nothing, so the size is the block `drawText` covers.
int Graphics2DBinding::measureText(lua_State* L) {
    const std::string_view content = lua::Stack::read<std::string_view>(L, 2);
    const haylen::text::Style style = lua::TypeConverter::readTextStyle(L, 3, {lua::TypeConverter::kDrawOrderFields});
    haylen::text::FontFamily* family = lua::Userdata::test<haylen::text::FontFamily>(L, 1);
    const haylen::math::Vec2 size = family != nullptr ? family->measure(content, style) : fontArgument(L, 1).measure(content, style);
    lua::Stack::push(L, size.x);
    lua::Stack::push(L, size.y);
    return 2;
}

// Draws a nine-slice with `drawNineSlice(slice, rect, color, options)`, whose options hold the border scale and the draw order.
int Graphics2DBinding::drawNineSlice(lua_State* L) {
    const haylen::math::Color color = lua_isnoneornil(L, 3) ? haylen::math::Color::white() : lua::Stack::read<haylen::math::Color>(L, 3);
    float borderScale = 1.0F;
    if (!lua_isnoneornil(L, 4)) {
        luaL_checktype(L, 4, LUA_TTABLE);
        // clang-format off
        lua::Table::readFields(L, 4, kNineSliceDrawFields, {lua::TypeConverter::kDrawOrderFields}, [L, &borderScale](std::string_view key) {
            lua::Table::readValue(L, key, borderScale);
        });
        // clang-format on
    }

    getRenderer(L).drawNineSlice(lua::Userdata::check<haylen::graphics2d::NineSlice>(L, 1), lua::Stack::read<haylen::math::Rect>(L, 2), color, lua::TypeConverter::readDrawOrder(L, 4, {kNineSliceDrawFields}), borderScale);
    return 0;
}

// Draws a `Light`, or a table with the properties of one, with `drawLight(light)`.
int Graphics2DBinding::drawLight(lua_State* L) {
    getRenderer(L).drawLight(lua::lighting2d::LightBinding::read(L, 1));
    return 0;
}

// Draws an `Occluder`, or a table with the properties of one, with `drawOccluder(occluder)`.
int Graphics2DBinding::drawOccluder(lua_State* L) {
    if (const haylen::lighting2d::Occluder* occluder = lua::Userdata::test<haylen::lighting2d::Occluder>(L, 1)) {
        getRenderer(L).drawOccluder(*occluder);
        return 0;
    }

    lua::lighting2d::OccluderBinding::read(L, 1, occluderScratch);
    getRenderer(L).drawOccluder(occluderScratch);
    return 0;
}

// Draws metaballs with `drawMetaballs({x1, y1, x2, y2, ...}, radius, {color, outlineColor, outlineWidth, threshold, layer, depth, ...})`, whose flat list of positions needs no table per point.
int Graphics2DBinding::drawMetaballs(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    const auto count = static_cast<std::size_t>(lua_rawlen(L, 1));
    luaL_argcheck(L, count % 2 == 0, 1, "metaball positions need two numbers per point");
    std::vector<haylen::math::Vec2>& points = pointScratch;
    points.resize(count / 2);
    for (std::size_t point = 0; point < points.size(); ++point) {
        lua_rawgeti(L, 1, static_cast<lua_Integer>(point * 2 + 1));
        lua_rawgeti(L, 1, static_cast<lua_Integer>(point * 2 + 2));
        points[point] = {lua::Converter<float>::fromNumber(L, luaL_checknumber(L, -2)), lua::Converter<float>::fromNumber(L, luaL_checknumber(L, -1))};
        lua_pop(L, 2);
    }

    haylen::graphics2d::Renderer::MetaballStyle style;
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        // clang-format off
        lua::Table::readFields(L, 3, kMetaballFields, {lua::TypeConverter::kDrawOrderFields}, [L, &style](std::string_view key) {
            if (key == "color") {
                lua::Table::readValue(L, key, style.color);
            } else if (key == "outlineColor") {
                lua::Table::readValue(L, key, style.outlineColor);
            } else if (key == "outlineWidth") {
                lua::Table::readValue(L, key, style.outlineWidth);
            } else if (key == "threshold") {
                lua::Table::readValue(L, key, style.threshold);
            }
        });
        // clang-format on
    }

    getRenderer(L).drawMetaballs(points, lua::Stack::read<float>(L, 2), style, lua::TypeConverter::readDrawOrder(L, 3, {kMetaballFields}));
    return 0;
}

// Blends two textures with `drawImageBlend(from, to, rect, {pattern = 'dissolve', progress = 0.5, center = {0.5, 0.5}, cellSize, blockSize, color, reversed, angle, layer, depth, ...})`.
int Graphics2DBinding::drawImageBlend(lua_State* L) {
    haylen::graphics2d::ImageBlend blend{.from = lua::Stack::read<haylen::graphics::Texture>(L, 1), .to = lua::Stack::read<haylen::graphics::Texture>(L, 2), .area = lua::Stack::read<haylen::math::Rect>(L, 3)};
    if (!lua_isnoneornil(L, 4)) {
        luaL_checktype(L, 4, LUA_TTABLE);
        // clang-format off
        lua::Table::readFields(L, 4, kImageBlendFields, {lua::TypeConverter::kDrawOrderFields}, [L, &blend](std::string_view key) {
            if (key == "pattern") {
                lua::Table::readValue(L, key, blend.pattern);
            } else if (key == "progress") {
                lua::Table::readValue(L, key, blend.progress);
            } else if (key == "center") {
                lua::Table::readValue(L, key, blend.center);
            } else if (key == "cellSize") {
                lua::Table::readValue(L, key, blend.cellSize);
            } else if (key == "blockSize") {
                lua::Table::readValue(L, key, blend.blockSize);
            } else if (key == "color") {
                lua::Table::readValue(L, key, blend.color);
            } else if (key == "reversed") {
                lua::Table::readValue(L, key, blend.reversed);
            } else if (key == "angle") {
                lua::Table::readValue(L, key, blend.angle);
            }
        });
        // clang-format on
    }

    getRenderer(L).drawImageBlend(blend, lua::TypeConverter::readDrawOrder(L, 4, {kImageBlendFields}));
    return 0;
}

int Graphics2DBinding::pushClip(lua_State* L) {
    getRenderer(L).pushClip(lua::Stack::read<haylen::math::Rect>(L, 1));
    return 0;
}

int Graphics2DBinding::popClip(lua_State* L) {
    getRenderer(L).popClip();
    return 0;
}

int Graphics2DBinding::pushFade(lua_State* L) {
    const auto rect = lua::Stack::read<haylen::math::Rect>(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    haylen::graphics2d::Renderer::Fade fade;
    // clang-format off
    lua::Table::readFields(L, 2, kFadeFields, {}, [L, &fade](std::string_view key) {
        if (key == "length") {
            lua::Table::readValue(L, key, fade.length);
        } else if (key == "left") {
            lua::Table::readValue(L, key, fade.left);
        } else if (key == "top") {
            lua::Table::readValue(L, key, fade.top);
        } else if (key == "right") {
            lua::Table::readValue(L, key, fade.right);
        } else {
            lua::Table::readValue(L, key, fade.bottom);
        }
    });
    // clang-format on
    getRenderer(L).pushFade(rect, fade);
    return 0;
}

int Graphics2DBinding::popFade(lua_State* L) {
    getRenderer(L).popFade();
    return 0;
}

int Graphics2DBinding::pushLayerOffset(lua_State* L) {
    getRenderer(L).pushLayerOffset(lua::Stack::read<int>(L, 1));
    return 0;
}

int Graphics2DBinding::popLayerOffset(lua_State* L) {
    getRenderer(L).popLayerOffset();
    return 0;
}

int Graphics2DBinding::stats(lua_State* L) {
    const haylen::graphics2d::Renderer::Stats& current = getRenderer(L).getStats();
    lua_createtable(L, 0, 12);
    const std::array<std::pair<const char*, std::size_t>, 12> values{{
        {"canvases", current.canvases},
        {"passes", current.passes},
        {"drawCalls", current.drawCalls},
        {"sprites", current.sprites},
        {"instances", current.instances},
        {"vertices", current.vertices},
        {"indices", current.indices},
        {"lights", current.lights},
        {"occluders", current.occluders},
        {"shadows", current.shadows},
        {"textureSwitches", current.textureSwitches},
        {"uploadedBytes", current.uploadedBytes},
    }};
    for (const auto& [name, value] : values) {
        lua::Stack::push(L, value);
        lua_setfield(L, -2, name);
    }

    return 1;
}

// Returns the textured quads and text blocks of the open canvas as a list of `{x, y, width, height, text, label}` bounds.
int Graphics2DBinding::drawn(lua_State* L) {
    lua_newtable(L);
    lua_Integer count = 0;
    // clang-format off
    getRenderer(L).visitDrawn([L, &count](const haylen::graphics2d::Renderer::Drawn& item) {
        const haylen::math::Rect bounds = haylen::math::Geometry2::bounds(item.corners);
        lua_createtable(L, 0, 6);
        lua::Stack::push(L, bounds.x);
        lua_setfield(L, -2, "x");
        lua::Stack::push(L, bounds.y);
        lua_setfield(L, -2, "y");
        lua::Stack::push(L, bounds.width);
        lua_setfield(L, -2, "width");
        lua::Stack::push(L, bounds.height);
        lua_setfield(L, -2, "height");
        lua::Stack::push(L, item.text);
        lua_setfield(L, -2, "text");
        if (!item.label.empty()) {
            lua::Stack::push(L, item.label);
            lua_setfield(L, -2, "label");
        }

        lua_rawseti(L, -2, ++count);
    });
    // clang-format on
    return 1;
}

int Graphics2DBinding::canvasBounds(lua_State* L) {
    lua::Stack::push(L, getRenderer(L).getCanvasBounds());
    return 1;
}

int Graphics2DBinding::canvasUnitSize(lua_State* L) {
    lua::Stack::push(L, getRenderer(L).getCanvasUnitSize());
    return 1;
}

int Graphics2DBinding::canvasLit(lua_State* L) {
    lua::Stack::push(L, getRenderer(L).isCanvasLit());
    return 1;
}

int Graphics2DBinding::capturing(lua_State* L) {
    lua::Stack::push(L, getRenderer(L).isCapturing());
    return 1;
}

int Graphics2DBinding::lightTexture(lua_State* L) {
    lua::Stack::push(L, getRenderer(L).getLightTexture());
    return 1;
}

int Graphics2DBinding::hdrLighting(lua_State* L) {
    lua::Stack::push(L, getRenderer(L).isHdrLighting());
    return 1;
}

int Graphics2DBinding::defaultFont(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getDefaultFont());
    return 1;
}

int Graphics2DBinding::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newSprite", &lua::Binding::native<&newSprite>}, {"newSpriteBatch", &lua::Binding::native<&newSpriteBatch>}, {"newTileLayer", &lua::Binding::native<&TileLayerBinding::create>}, {"newMesh", &lua::Binding::native<&MeshBinding::create>}, {"newCameraRig", &lua::Binding::native<&CameraRigBinding::create>}, {"newPixelCanvas", &lua::Binding::native<&PixelCanvasBinding::create>}, {"newGridMesh", &lua::Binding::native<&MeshBinding::createGrid>}, {"newPath", &lua::Binding::native<&PathBinding::create>}, {"newCamera", &newCamera}, {"newNineSlice", &lua::Binding::native<&newNineSlice>}, {"newParallax", &lua::Binding::native<&newParallax>}, {"blendCameras", &blendCameras}, {"beginWorld", &lua::Binding::native<&beginWorld>}, {"beginScreen", &lua::Binding::native<&beginScreen>}, {"beginTarget", &lua::Binding::native<&beginTarget>}, {"beginCapture", &lua::Binding::native<&beginCapture>}, {"endCapture", &lua::Binding::native<&endCapture>}, {"draw", &lua::Binding::native<&draw>}, {"drawVector", &lua::Binding::native<&drawVector>}, {"drawBatch", &lua::Binding::native<&drawBatch>}, {"drawStatic", &lua::Binding::native<&drawStatic>}, {"drawRect", &lua::Binding::native<&drawRect>}, {"drawRectOutline", &lua::Binding::native<&drawRectOutline>}, {"drawLine", &lua::Binding::native<&drawLine>}, {"drawCircle", &lua::Binding::native<&drawCircle>}, {"drawEllipse", &lua::Binding::native<&drawEllipse>}, {"drawPoints", &lua::Binding::native<&drawPoints>}, {"drawRing", &lua::Binding::native<&drawRing>}, {"drawArc", &lua::Binding::native<&drawArc>}, {"drawShape", &lua::Binding::native<&drawShape>}, {"drawGlass", &lua::Binding::native<&GlassBinding::drawGlass>}, {"drawPolygon", &lua::Binding::native<&drawPolygon>}, {"drawPolyline", &lua::Binding::native<&drawPolyline>}, {"drawMesh", &lua::Binding::native<&drawMesh>}, {"drawStroke", &lua::Binding::native<&StrokeBinding::drawStroke>}, {"drawText", &lua::Binding::native<&drawText>}, {"measureText", &lua::Binding::native<&measureText>}, {"drawNineSlice", &lua::Binding::native<&drawNineSlice>}, {"drawLight", &lua::Binding::native<&drawLight>}, {"drawOccluder", &lua::Binding::native<&drawOccluder>}, {"drawMetaballs", &lua::Binding::native<&drawMetaballs>}, {"newMaterial", &lua::Binding::native<&MaterialBinding::newMaterial>}, {"drawImageBlend", &lua::Binding::native<&drawImageBlend>}, {"pushClip", &lua::Binding::native<&pushClip>}, {"popClip", &lua::Binding::native<&popClip>}, {"pushFade", &lua::Binding::native<&pushFade>}, {"popFade", &lua::Binding::native<&popFade>}, {"pushLayerOffset", &lua::Binding::native<&pushLayerOffset>}, {"beginGroup", &lua::Binding::native<&GroupBinding::beginGroup>}, {"endGroup", &lua::Binding::native<&GroupBinding::endGroup>}, {"pushMask", &GroupBinding::pushMask}, {"popMask", &lua::Binding::native<&GroupBinding::popMask>}, {"popLayerOffset", &lua::Binding::native<&popLayerOffset>}, {"stats", &stats}, {"drawn", &lua::Binding::native<&drawn>}, {"canvasBounds", &canvasBounds}, {"canvasUnitSize", &lua::Binding::native<&canvasUnitSize>}, {"canvasLit", &lua::Binding::native<&canvasLit>}, {"capturing", &capturing}, {"lightTexture", &lightTexture}, {"hdrLighting", &hdrLighting}, {"defaultFont", &defaultFont}, {"newRichText", &lua::Binding::native<&RichTextBinding::newRichText>}, {"drawRichText", &lua::Binding::native<&RichTextBinding::drawRichText>}, {"measureRichText", &lua::Binding::native<&RichTextBinding::measureRichText>}, {"registerTextEffect", &lua::Binding::native<&RichTextBinding::registerTextEffect>}, {"registerTextIcon", &lua::Binding::native<&RichTextBinding::registerTextIcon>}, {"textEffectNames", &lua::Binding::native<&RichTextBinding::textEffectNames>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void Graphics2DBinding::install(lua_State* L) {
    NineSliceBinding::install(L);
    MaterialBinding::install(L);
    SpriteBatchBinding::install(L);
    TileLayerBinding::install(L);
    MeshBinding::install(L);
    PathBinding::install(L);
    CameraRigBinding::install(L);
    PixelCanvasBinding::install(L);
    SpriteBinding::install(L);
    CameraBinding::install(L);
    ParallaxBinding::install(L);
    RichTextBinding::install(L);
    lua::Binding::preload(L, "haylen.graphics2d", &open);
}

} // namespace haylen::lua::graphics2d
