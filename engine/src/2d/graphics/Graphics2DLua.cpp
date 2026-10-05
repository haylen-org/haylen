#include "2d/graphics/Graphics2DLua.hpp"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "2d/graphics/CameraLua.hpp"
#include "2d/graphics/MaterialLua.hpp"
#include "2d/graphics/NineSliceLua.hpp"
#include "2d/graphics/ParallaxLua.hpp"
#include "2d/graphics/RichTextLua.hpp"
#include "2d/graphics/SpriteBatchLua.hpp"
#include "2d/graphics/SpriteLua.hpp"
#include "2d/lighting/LightLua.hpp"
#include "2d/lighting/OccluderLua.hpp"
#include "core/FloatBufferLua.hpp"
#include "graphics/FontLua.hpp"
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
#include "haylen/math/Geometry.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/text/Font.hpp"
#include "haylen/text/FontFamily.hpp"

namespace haylen::graphics2d {

Renderer& Graphics2DLua::getRenderer(lua_State* L) {
    return lua::Runtime::getEngine(L).getRenderer2D();
}

text::Font& Graphics2DLua::fontArgument(lua_State* L, int index) {
    return lua_isnoneornil(L, index) ? *lua::Runtime::getEngine(L).getDefaultFont() : lua::Userdata::check<text::Font>(L, index);
}

Renderer::CanvasOptions Graphics2DLua::readCanvasOptions(lua_State* L, int index) {
    Renderer::CanvasOptions options;
    if (lua_isnoneornil(L, index)) {
        return options;
    }
    luaL_checktype(L, index, LUA_TTABLE);
    const int table = lua_absindex(L, index);
    lua::Table::checkFields(L, table, {kCanvasFields});
    lua::Table::readField(L, table, "sort", options.sort);
    lua::Table::readField(L, table, "order", options.order);
    lua::Table::readField(L, table, "visibilityMask", options.visibilityMask);
    lua::Table::readField(L, table, "ambientLight", options.ambientLight);
    lua::Table::readField(L, table, "clear", options.clear);

    lua_getfield(L, table, "postProcess");
    if (!lua_isnil(L, -1)) {
        luaL_checktype(L, -1, LUA_TTABLE);
        const int post = lua_gettop(L);
        lua::Table::checkFields(L, post, {kPostProcessFields});
        PostProcess process;
        lua::Table::readField(L, post, "tint", process.tint);
        lua::Table::readField(L, post, "saturation", process.saturation);
        lua::Table::readField(L, post, "brightness", process.brightness);
        lua::Table::readField(L, post, "contrast", process.contrast);
        lua::Table::readField(L, post, "vignetteStrength", process.vignetteStrength);
        lua::Table::readField(L, post, "vignetteRadius", process.vignetteRadius);
        lua::Table::readField(L, post, "vignetteSoftness", process.vignetteSoftness);
        lua::Table::readField(L, post, "fade", process.fade);
        lua::Table::readField(L, post, "distortion", process.distortion);
        lua::Table::readField(L, post, "chromaticAberration", process.chromaticAberration);
        lua::Table::readField(L, post, "pixelate", process.pixelate);
        lua::Table::readField(L, post, "blur", process.blur);
        lua::Table::readField(L, post, "bloomStrength", process.bloomStrength);
        lua::Table::readField(L, post, "bloomThreshold", process.bloomThreshold);
        lua::Table::readField(L, post, "bloomRadius", process.bloomRadius);
        lua::Table::readField(L, post, "colorLut", process.colorLut);
        lua::Table::readField(L, post, "colorLutStrength", process.colorLutStrength);
        lua::Table::readField(L, post, "materials", process.materials);
        options.postProcess = process;
    }
    lua_pop(L, 1);
    return options;
}

void Graphics2DLua::assignFields(lua_State* L, int options) {
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
int Graphics2DLua::newSprite(lua_State* L) {
    Sprite sprite;
    sprite.texture = lua::Stack::read<graphics::Texture>(L, 1);
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
    }
    lua_settop(L, 2);
    lua::Userdata::emplace<Sprite>(L, std::move(sprite));
    assignFields(L, 2);
    return 1;
}

// Creates a parallax layer with `newParallax(texture, {scrollScale = {0.5, 0.5}, repeatX = true, autoscroll = {-20, 0}, ...})`.
int Graphics2DLua::newParallax(lua_State* L) {
    Parallax parallax;
    parallax.texture = lua::Stack::read<graphics::Texture>(L, 1);
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
    }
    lua_settop(L, 2);
    lua::Userdata::emplace<Parallax>(L, std::move(parallax));
    assignFields(L, 2);
    return 1;
}

int Graphics2DLua::blendCameras(lua_State* L) {
    lua::Userdata::emplace<Camera>(L, Camera::blend(lua::Userdata::check<Camera>(L, 1), lua::Userdata::check<Camera>(L, 2), lua::Stack::read<float>(L, 3)));
    return 1;
}

int Graphics2DLua::newSpriteBatch(lua_State* L) {
    lua::Userdata::emplace<SpriteBatch>(L, std::make_shared<SpriteBatch>(lua::Stack::read<graphics::Texture>(L, 1)));
    return 1;
}

int Graphics2DLua::newCamera(lua_State* L) {
    lua::Userdata::emplace<Camera>(L);
    return 1;
}

// Builds a nine-slice with `newNineSlice(texture, {source = rect, borders = {left, top, right, bottom}})` or `newNineSlice(texture, {pieces = {nine rects}})`.
int Graphics2DLua::newNineSlice(lua_State* L) {
    const graphics::Texture texture = lua::Stack::read<graphics::Texture>(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    lua::Table::checkFields(L, 2, {kNineSliceFields});

    NineSlice slice;
    lua_getfield(L, 2, "pieces");
    if (!lua_isnil(L, -1)) {
        const std::vector<math::Rect> pieces = lua::Stack::read<std::vector<math::Rect>>(L, -1);
        luaL_argcheck(L, pieces.size() == 9, 2, "a nine-slice needs exactly nine pieces");
        std::array<math::Rect, 9> regions{};
        std::copy(pieces.begin(), pieces.end(), regions.begin());
        slice = NineSlice::fromPieces(texture, regions);
    } else {
        math::Rect source{};
        std::array<float, 4> borders{};
        lua::Table::readField(L, 2, "source", source);
        lua_getfield(L, 2, "borders");
        const std::vector<float> values = lua::Stack::read<std::vector<float>>(L, -1);
        luaL_argcheck(L, values.size() == 4, 2, "borders need left, top, right and bottom");
        std::copy(values.begin(), values.end(), borders.begin());
        lua_pop(L, 1);
        slice = NineSlice::fromBorders(texture, source, {borders[0], borders[1], borders[2], borders[3]});
    }
    lua_pop(L, 1);

    lua::Table::readField(L, 2, "fill", slice.fill);
    lua::Userdata::emplace<NineSlice>(L, std::move(slice));
    return 1;
}

int Graphics2DLua::beginWorld(lua_State* L) {
    getRenderer(L).beginWorld(lua::Userdata::check<Camera>(L, 1), readCanvasOptions(L, 2));
    return 0;
}

int Graphics2DLua::beginScreen(lua_State* L) {
    getRenderer(L).beginScreen(readCanvasOptions(L, 1));
    return 0;
}

int Graphics2DLua::beginTarget(lua_State* L) {
    getRenderer(L).beginTarget(lua::Userdata::check<graphics::RenderTarget>(L, 1), lua::Userdata::check<Camera>(L, 2), readCanvasOptions(L, 3));
    return 0;
}

// Captures the following canvases with `beginCapture(target[, clear])`, where the clear color is transparent unless given.
int Graphics2DLua::beginCapture(lua_State* L) {
    const math::Color clear = lua_isnoneornil(L, 2) ? math::Color::transparent() : lua::Stack::read<math::Color>(L, 2);
    getRenderer(L).beginCapture(lua::Userdata::check<graphics::RenderTarget>(L, 1), clear);
    return 0;
}

int Graphics2DLua::endCapture(lua_State* L) {
    getRenderer(L).endCapture();
    return 0;
}

// Draws a texture immediately with `draw(texture, x, y, {source, width, height, scaleX, scaleY, pivotX, pivotY, rotation, color, flash, flipHorizontal, flipVertical, flipDiagonal, layer, depth, blend})`.
int Graphics2DLua::draw(lua_State* L) {
    Sprite sprite;
    sprite.texture = lua::Stack::read<graphics::Texture>(L, 1);
    sprite.position = {lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)};
    if (!lua_isnoneornil(L, 4)) {
        const SpriteInstance instance = lua::TypeConverter::readSpriteInstance(L, 4, sprite.texture, {.position = sprite.position}, {kScaleFields, lua::TypeConverter::kDrawOrderFields, SpriteBatchLua::kPartColorFields, kEffectFields});
        sprite.position = instance.position;
        sprite.source = instance.source;
        sprite.size = instance.size;
        sprite.pivot = instance.pivot;
        sprite.rotation = instance.rotation;
        sprite.color = instance.color;
        sprite.flash = instance.flash;
        sprite.flip = instance.flip;
        lua::Table::readField(L, 4, "scaleX", sprite.scale.x);
        lua::Table::readField(L, 4, "scaleY", sprite.scale.y);
        lua::Table::readField(L, 4, "partColors", sprite.partColors);
        lua::Table::readField(L, 4, "effect", sprite.effect);
        sprite.order = lua::TypeConverter::readDrawOrder(L, 4, {lua::TypeConverter::kSpriteInstanceFields, kScaleFields, SpriteBatchLua::kPartColorFields, kEffectFields});
    }
    getRenderer(L).draw(sprite);
    return 0;
}

// Draws a vector image with `drawVector(image, x, y[, options])`, at the size of the image unless the options give one, where the options take the keys of `draw` but `source` and `partColors`.
int Graphics2DLua::drawVector(lua_State* L) {
    const graphics::VectorImage& image = lua::Userdata::check<graphics::VectorImage>(L, 1);
    Sprite sprite;
    sprite.position = {lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)};
    if (!lua_isnoneornil(L, 4)) {
        luaL_checktype(L, 4, LUA_TTABLE);
        lua::Table::checkFields(L, 4, {kVectorFields, lua::TypeConverter::kDrawOrderFields});
        lua::Table::readField(L, 4, "x", sprite.position.x);
        lua::Table::readField(L, 4, "y", sprite.position.y);
        lua::Table::readField(L, 4, "width", sprite.size.x);
        lua::Table::readField(L, 4, "height", sprite.size.y);
        lua::Table::readField(L, 4, "scaleX", sprite.scale.x);
        lua::Table::readField(L, 4, "scaleY", sprite.scale.y);
        lua::Table::readField(L, 4, "pivotX", sprite.pivot.x);
        lua::Table::readField(L, 4, "pivotY", sprite.pivot.y);
        lua::Table::readField(L, 4, "rotation", sprite.rotation);
        lua::Table::readField(L, 4, "color", sprite.color);
        lua::Table::readField(L, 4, "flash", sprite.flash);
        lua::Table::readField(L, 4, "flipHorizontal", sprite.flip.horizontal);
        lua::Table::readField(L, 4, "flipVertical", sprite.flip.vertical);
        lua::Table::readField(L, 4, "flipDiagonal", sprite.flip.diagonal);
        sprite.order = lua::TypeConverter::readDrawOrder(L, 4, {kVectorFields});
    }
    getRenderer(L).drawVector(image, sprite);
    return 0;
}

// Draws a list of sprite tables that share one texture as a single batch with `drawBatch(texture, {{x, y, width, height, ...}, ...}, order)`, or the sprites a float buffer holds with `drawBatch(texture, buffer, {fields = {'x', 'y', ...}, width = ..., ...}, order)`, where the table is the template of every sprite.
int Graphics2DLua::drawBatch(lua_State* L) {
    const graphics::Texture texture = lua::Stack::read<graphics::Texture>(L, 1);
    if (const core::FloatBuffer* buffer = lua::Userdata::test<core::FloatBuffer>(L, 2)) {
        const SpriteInstance sprite = lua::TypeConverter::readSpriteInstance(L, 3, texture, {}, {SpriteBatchLua::kLayoutFields});
        if (lua_getfield(L, 3, "fields") != LUA_TTABLE) {
            return luaL_error(L, "A \"drawBatch\" call with a float buffer needs the fields each sprite takes, such as \"fields = {'x', 'y'}\".");
        }
        const SpriteLayout layout = SpriteBatchLua::readLayout(L, -1, sprite);
        lua_pop(L, 1);
        getRenderer(L).drawBatch(texture, buffer->getValues(), layout, lua::TypeConverter::readDrawOrder(L, 4));
        return 0;
    }
    luaL_checktype(L, 2, LUA_TTABLE);
    std::vector<SpriteInstance> sprites(static_cast<std::size_t>(luaL_len(L, 2)));
    std::vector<PartColors> partColors;
    for (std::size_t index = 0; index < sprites.size(); ++index) {
        lua_rawgeti(L, 2, static_cast<lua_Integer>(index + 1));
        sprites[index] = lua::TypeConverter::readSpriteInstance(L, -1, texture, {}, {SpriteBatchLua::kPartColorFields});
        if (lua_getfield(L, -1, "partColors") != LUA_TNIL) {
            partColors.resize(sprites.size());
            partColors[index] = lua::Stack::read<PartColors>(L, -1);
        }
        lua_pop(L, 2);
    }
    getRenderer(L).drawBatch(texture, sprites, lua::TypeConverter::readDrawOrder(L, 3), partColors);
    return 0;
}

// Draws a baked batch with `drawStatic(batch[, x, y[, order]])`, where `x` and `y` shift the whole batch without rebaking it.
int Graphics2DLua::drawStatic(lua_State* L) {
    const math::Vec2 offset{static_cast<float>(luaL_optnumber(L, 2, 0.0)), static_cast<float>(luaL_optnumber(L, 3, 0.0))};
    getRenderer(L).drawStatic(lua::Userdata::check<StaticSpriteBatch>(L, 1), lua::TypeConverter::readDrawOrder(L, 4), offset);
    return 0;
}

int Graphics2DLua::drawRect(lua_State* L) {
    getRenderer(L).drawRect(lua::Stack::read<math::Rect>(L, 1), lua::Stack::read<math::Color>(L, 2), lua::TypeConverter::readDrawOrder(L, 3));
    return 0;
}

int Graphics2DLua::drawRectOutline(lua_State* L) {
    getRenderer(L).drawRectOutline(lua::Stack::read<math::Rect>(L, 1), lua::Stack::read<float>(L, 2), lua::Stack::read<math::Color>(L, 3), lua::TypeConverter::readDrawOrder(L, 4));
    return 0;
}

int Graphics2DLua::drawLine(lua_State* L) {
    getRenderer(L).drawLine({lua::Stack::read<float>(L, 1), lua::Stack::read<float>(L, 2)}, {lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)}, lua::Stack::read<float>(L, 5), lua::Stack::read<math::Color>(L, 6), lua::TypeConverter::readDrawOrder(L, 7));
    return 0;
}

int Graphics2DLua::drawCircle(lua_State* L) {
    getRenderer(L).drawCircle({lua::Stack::read<float>(L, 1), lua::Stack::read<float>(L, 2)}, lua::Stack::read<float>(L, 3), lua::Stack::read<math::Color>(L, 4), lua::TypeConverter::readDrawOrder(L, 5));
    return 0;
}

int Graphics2DLua::drawRing(lua_State* L) {
    getRenderer(L).drawRing({lua::Stack::read<float>(L, 1), lua::Stack::read<float>(L, 2)}, lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4), lua::Stack::read<math::Color>(L, 5), lua::TypeConverter::readDrawOrder(L, 6));
    return 0;
}

int Graphics2DLua::drawArc(lua_State* L) {
    getRenderer(L).drawArc({lua::Stack::read<float>(L, 1), lua::Stack::read<float>(L, 2)}, lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4), lua::Stack::read<float>(L, 5), lua::Stack::read<float>(L, 6), lua::Stack::read<math::Color>(L, 7), lua::TypeConverter::readDrawOrder(L, 8));
    return 0;
}

// Reads the value on top of the stack as one radius for every corner or as four from the top-left corner clockwise.
void Graphics2DLua::readRadii(lua_State* L, std::array<float, 4>& radii) {
    if (lua_type(L, -1) == LUA_TNUMBER) {
        radii.fill(static_cast<float>(lua_tonumber(L, -1)));
        return;
    }
    std::vector<float> corners;
    lua::Table::readValue(L, "radius", corners);
    if (corners.size() != radii.size()) {
        throw std::invalid_argument("The option \"radius\" of a shape takes one radius for every corner or four, from the top-left corner clockwise.");
    }
    std::ranges::copy(corners, radii.begin());
}

// Draws a rounded rectangle, a circle, a capsule, a ring or an arc with `drawShape(rect, {radius, rotation, startAngle, sweep, color, borderWidth, borderColor, softness, layer, depth, ...})`.
int Graphics2DLua::drawShape(lua_State* L) {
    Shape shape{.bounds = lua::Stack::read<math::Rect>(L, 1)};
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        // clang-format off
        lua::Table::readFields(L, 2, kShapeFields, {lua::TypeConverter::kDrawOrderFields}, [L, &shape](std::string_view key) {
            if (key == "radius") {
                readRadii(L, shape.radii);
            } else if (key == "rotation") {
                lua::Table::readValue(L, key, shape.rotation);
            } else if (key == "startAngle") {
                lua::Table::readValue(L, key, shape.startAngle);
            } else if (key == "sweep") {
                lua::Table::readValue(L, key, shape.sweep);
            } else if (key == "color") {
                lua::Table::readValue(L, key, shape.color);
            } else if (key == "borderWidth") {
                lua::Table::readValue(L, key, shape.borderWidth);
            } else if (key == "borderColor") {
                lua::Table::readValue(L, key, shape.borderColor);
            } else if (key == "softness") {
                lua::Table::readValue(L, key, shape.softness);
            }
        });
        // clang-format on
    }
    getRenderer(L).drawShape(shape, lua::TypeConverter::readDrawOrder(L, 2, {kShapeFields}));
    return 0;
}

int Graphics2DLua::drawPolygon(lua_State* L) {
    getRenderer(L).drawPolygon(lua::Stack::read<std::vector<math::Vec2>>(L, 1), lua::Stack::read<math::Color>(L, 2), lua::TypeConverter::readDrawOrder(L, 3));
    return 0;
}

int Graphics2DLua::drawPolyline(lua_State* L) {
    getRenderer(L).drawPolyline(lua::Stack::read<std::vector<math::Vec2>>(L, 1), lua::Stack::read<float>(L, 2), lua::Stack::read<math::Color>(L, 3), lua_toboolean(L, 4) != 0, lua::TypeConverter::readDrawOrder(L, 5));
    return 0;
}

// Draws triangles with `drawMesh(texture or nil, {{x, y, u, v, color}, ...}, {1, 2, 3, ...}, order)`.
int Graphics2DLua::drawMesh(lua_State* L) {
    const graphics::Texture texture = lua_isnil(L, 1) ? graphics::Texture{} : lua::Stack::read<graphics::Texture>(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    std::vector<MeshVertex> vertices(static_cast<std::size_t>(luaL_len(L, 2)));
    for (std::size_t index = 0; index < vertices.size(); ++index) {
        lua_rawgeti(L, 2, static_cast<lua_Integer>(index + 1));
        luaL_checktype(L, -1, LUA_TTABLE);
        const int vertex = lua_gettop(L);
        lua::Table::checkFields(L, vertex, {kMeshVertexFields});
        lua::Table::readField(L, vertex, "x", vertices[index].position.x);
        lua::Table::readField(L, vertex, "y", vertices[index].position.y);
        lua::Table::readField(L, vertex, "u", vertices[index].uv.x);
        lua::Table::readField(L, vertex, "v", vertices[index].uv.y);
        lua::Table::readField(L, vertex, "color", vertices[index].color);
        lua_pop(L, 1);
    }

    std::vector<std::uint32_t> indices = lua::Stack::read<std::vector<std::uint32_t>>(L, 3);
    for (std::uint32_t& index : indices) {
        luaL_argcheck(L, index >= 1, 3, "mesh indices start at 1");
        --index;
    }
    getRenderer(L).drawMesh(texture, vertices, indices, lua::TypeConverter::readDrawOrder(L, 4));
    return 0;
}

// Draws text with `drawText(font, text, x, y, style)`, where the font is a `Font`, a `FontFamily` or `nil` for the default font, and the style also takes the layer, depth and blend of the draw. A family draws what its faces lack from its fallbacks.
int Graphics2DLua::drawText(lua_State* L) {
    const std::string_view content = lua::Stack::read<std::string_view>(L, 2);
    const math::Vec2 position{lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)};
    const text::Style style = lua::TypeConverter::readTextStyle(L, 5, {lua::TypeConverter::kDrawOrderFields});
    const DrawOrder order = lua::TypeConverter::readDrawOrder(L, 5, {lua::TypeConverter::kTextStyleFields});
    if (text::FontFamily* family = lua::Userdata::test<text::FontFamily>(L, 1)) {
        getRenderer(L).drawText(*family, content, position, style, order);
    } else {
        getRenderer(L).drawText(fontArgument(L, 1), content, position, style, order);
    }
    return 0;
}

// Measures text with the same font or family and style table `drawText` takes, whose draw order keys change nothing, so the size is the block `drawText` covers.
int Graphics2DLua::measureText(lua_State* L) {
    const std::string_view content = lua::Stack::read<std::string_view>(L, 2);
    const text::Style style = lua::TypeConverter::readTextStyle(L, 3, {lua::TypeConverter::kDrawOrderFields});
    text::FontFamily* family = lua::Userdata::test<text::FontFamily>(L, 1);
    const math::Vec2 size = family != nullptr ? family->measure(content, style) : fontArgument(L, 1).measure(content, style);
    lua::Stack::push(L, size.x);
    lua::Stack::push(L, size.y);
    return 2;
}

int Graphics2DLua::drawNineSlice(lua_State* L) {
    const math::Color color = lua_isnoneornil(L, 3) ? math::Color::white() : lua::Stack::read<math::Color>(L, 3);
    getRenderer(L).drawNineSlice(lua::Userdata::check<NineSlice>(L, 1), lua::Stack::read<math::Rect>(L, 2), color, lua::TypeConverter::readDrawOrder(L, 4), static_cast<float>(luaL_optnumber(L, 5, 1.0)));
    return 0;
}

// Draws a `Light`, or a table with the properties of one, with `drawLight(light)`.
int Graphics2DLua::drawLight(lua_State* L) {
    getRenderer(L).drawLight(lighting2d::LightLua::read(L, 1));
    return 0;
}

// Draws an `Occluder`, or a table with the properties of one, with `drawOccluder(occluder)`.
int Graphics2DLua::drawOccluder(lua_State* L) {
    getRenderer(L).drawOccluder(lighting2d::OccluderLua::read(L, 1));
    return 0;
}

// Draws metaballs with `drawMetaballs({x1, y1, x2, y2, ...}, radius, {color, outlineColor, outlineWidth, threshold, layer, depth, ...})`, whose flat list of positions needs no table per point.
int Graphics2DLua::drawMetaballs(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    const auto count = static_cast<std::size_t>(luaL_len(L, 1));
    luaL_argcheck(L, count % 2 == 0, 1, "metaball positions need two numbers per point");
    std::vector<math::Vec2> points(count / 2);
    for (std::size_t point = 0; point < points.size(); ++point) {
        lua_rawgeti(L, 1, static_cast<lua_Integer>(point * 2 + 1));
        lua_rawgeti(L, 1, static_cast<lua_Integer>(point * 2 + 2));
        points[point] = {static_cast<float>(luaL_checknumber(L, -2)), static_cast<float>(luaL_checknumber(L, -1))};
        lua_pop(L, 2);
    }

    Renderer::MetaballStyle style;
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua::Table::checkFields(L, 3, {kMetaballFields, lua::TypeConverter::kDrawOrderFields});
        lua::Table::readField(L, 3, "color", style.color);
        lua::Table::readField(L, 3, "outlineColor", style.outlineColor);
        lua::Table::readField(L, 3, "outlineWidth", style.outlineWidth);
        lua::Table::readField(L, 3, "threshold", style.threshold);
    }
    getRenderer(L).drawMetaballs(points, lua::Stack::read<float>(L, 2), style, lua::TypeConverter::readDrawOrder(L, 3, {kMetaballFields}));
    return 0;
}

// Blends two textures with `drawImageBlend(from, to, rect, {pattern = 'dissolve', progress = 0.5, center = {0.5, 0.5}, cellSize, blockSize, color, reversed, angle, layer, depth, ...})`.
int Graphics2DLua::drawImageBlend(lua_State* L) {
    ImageBlend blend{.from = lua::Stack::read<graphics::Texture>(L, 1), .to = lua::Stack::read<graphics::Texture>(L, 2), .area = lua::Stack::read<math::Rect>(L, 3)};
    if (!lua_isnoneornil(L, 4)) {
        luaL_checktype(L, 4, LUA_TTABLE);
        lua::Table::checkFields(L, 4, {kImageBlendFields, lua::TypeConverter::kDrawOrderFields});
        lua::Table::readField(L, 4, "pattern", blend.pattern);
        lua::Table::readField(L, 4, "progress", blend.progress);
        lua::Table::readField(L, 4, "center", blend.center);
        lua::Table::readField(L, 4, "cellSize", blend.cellSize);
        lua::Table::readField(L, 4, "blockSize", blend.blockSize);
        lua::Table::readField(L, 4, "color", blend.color);
        lua::Table::readField(L, 4, "reversed", blend.reversed);
        lua::Table::readField(L, 4, "angle", blend.angle);
    }
    getRenderer(L).drawImageBlend(blend, lua::TypeConverter::readDrawOrder(L, 4, {kImageBlendFields}));
    return 0;
}

int Graphics2DLua::pushClip(lua_State* L) {
    getRenderer(L).pushClip(lua::Stack::read<math::Rect>(L, 1));
    return 0;
}

int Graphics2DLua::popClip(lua_State* L) {
    getRenderer(L).popClip();
    return 0;
}

int Graphics2DLua::pushLayerOffset(lua_State* L) {
    getRenderer(L).pushLayerOffset(lua::Stack::read<int>(L, 1));
    return 0;
}

int Graphics2DLua::popLayerOffset(lua_State* L) {
    getRenderer(L).popLayerOffset();
    return 0;
}

int Graphics2DLua::stats(lua_State* L) {
    const Renderer::Stats& current = getRenderer(L).getStats();
    lua_createtable(L, 0, 12);
    const std::pair<const char*, std::size_t> values[] = {
        {"canvases", current.canvases}, {"passes", current.passes}, {"drawCalls", current.drawCalls}, {"sprites", current.sprites}, {"instances", current.instances}, {"vertices", current.vertices}, {"indices", current.indices}, {"lights", current.lights}, {"occluders", current.occluders}, {"shadows", current.shadows}, {"textureSwitches", current.textureSwitches}, {"uploadedBytes", current.uploadedBytes},
    };
    for (const auto& [name, value] : values) {
        lua::Stack::push(L, value);
        lua_setfield(L, -2, name);
    }
    return 1;
}

// Returns the textured quads and text blocks of the open canvas as a list of `{x, y, width, height, text, label}` bounds.
int Graphics2DLua::drawn(lua_State* L) {
    lua_newtable(L);
    lua_Integer count = 0;
    // clang-format off
    getRenderer(L).visitDrawn([L, &count](const Renderer::Drawn& item) {
        const math::Rect bounds = math::Geometry::bounds(item.corners);
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

int Graphics2DLua::canvasBounds(lua_State* L) {
    lua::Stack::push(L, getRenderer(L).getCanvasBounds());
    return 1;
}

int Graphics2DLua::canvasUnitSize(lua_State* L) {
    lua::Stack::push(L, getRenderer(L).getCanvasUnitSize());
    return 1;
}

int Graphics2DLua::canvasLit(lua_State* L) {
    lua::Stack::push(L, getRenderer(L).isCanvasLit());
    return 1;
}

int Graphics2DLua::capturing(lua_State* L) {
    lua::Stack::push(L, getRenderer(L).isCapturing());
    return 1;
}

int Graphics2DLua::lightTexture(lua_State* L) {
    lua::Stack::push(L, getRenderer(L).getLightTexture());
    return 1;
}

int Graphics2DLua::hdrLighting(lua_State* L) {
    lua::Stack::push(L, getRenderer(L).isHdrLighting());
    return 1;
}

int Graphics2DLua::defaultFont(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getDefaultFont());
    return 1;
}

int Graphics2DLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newSprite", &lua::Binding::native<&newSprite>}, {"newSpriteBatch", &lua::Binding::native<&newSpriteBatch>}, {"newCamera", &newCamera}, {"newNineSlice", &lua::Binding::native<&newNineSlice>}, {"newParallax", &lua::Binding::native<&newParallax>}, {"blendCameras", &blendCameras}, {"beginWorld", &lua::Binding::native<&beginWorld>}, {"beginScreen", &lua::Binding::native<&beginScreen>}, {"beginTarget", &lua::Binding::native<&beginTarget>}, {"beginCapture", &lua::Binding::native<&beginCapture>}, {"endCapture", &lua::Binding::native<&endCapture>}, {"draw", &lua::Binding::native<&draw>}, {"drawVector", &lua::Binding::native<&drawVector>}, {"drawBatch", &lua::Binding::native<&drawBatch>}, {"drawStatic", &lua::Binding::native<&drawStatic>}, {"drawRect", &lua::Binding::native<&drawRect>}, {"drawRectOutline", &lua::Binding::native<&drawRectOutline>}, {"drawLine", &lua::Binding::native<&drawLine>}, {"drawCircle", &lua::Binding::native<&drawCircle>}, {"drawRing", &lua::Binding::native<&drawRing>}, {"drawArc", &lua::Binding::native<&drawArc>}, {"drawShape", &lua::Binding::native<&drawShape>}, {"drawPolygon", &lua::Binding::native<&drawPolygon>}, {"drawPolyline", &lua::Binding::native<&drawPolyline>}, {"drawMesh", &lua::Binding::native<&drawMesh>}, {"drawText", &lua::Binding::native<&drawText>}, {"measureText", &lua::Binding::native<&measureText>}, {"drawNineSlice", &lua::Binding::native<&drawNineSlice>}, {"drawLight", &lua::Binding::native<&drawLight>}, {"drawOccluder", &lua::Binding::native<&drawOccluder>}, {"drawMetaballs", &lua::Binding::native<&drawMetaballs>}, {"newMaterial", &lua::Binding::native<&MaterialLua::newMaterial>}, {"drawImageBlend", &lua::Binding::native<&drawImageBlend>}, {"pushClip", &lua::Binding::native<&pushClip>}, {"popClip", &lua::Binding::native<&popClip>}, {"pushLayerOffset", &lua::Binding::native<&pushLayerOffset>}, {"popLayerOffset", &lua::Binding::native<&popLayerOffset>}, {"stats", &stats}, {"drawn", &lua::Binding::native<&drawn>}, {"canvasBounds", &canvasBounds}, {"canvasUnitSize", &lua::Binding::native<&canvasUnitSize>}, {"canvasLit", &lua::Binding::native<&canvasLit>}, {"capturing", &capturing}, {"lightTexture", &lightTexture}, {"hdrLighting", &hdrLighting}, {"defaultFont", &defaultFont}, {"newRichText", &lua::Binding::native<&RichTextLua::newRichText>}, {"drawRichText", &lua::Binding::native<&RichTextLua::drawRichText>}, {"measureRichText", &lua::Binding::native<&RichTextLua::measureRichText>}, {"registerTextEffect", &lua::Binding::native<&RichTextLua::registerTextEffect>}, {"registerTextIcon", &lua::Binding::native<&RichTextLua::registerTextIcon>}, {"textEffectNames", &lua::Binding::native<&RichTextLua::textEffectNames>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void Graphics2DLua::install(lua_State* L) {
    NineSliceLua::install(L);
    MaterialLua::install(L);
    SpriteBatchLua::install(L);
    SpriteLua::install(L);
    CameraLua::install(L);
    ParallaxLua::install(L);
    RichTextLua::install(L);
    lua::Binding::preload(L, "haylen.graphics2d", &open);
}

} // namespace haylen::graphics2d
