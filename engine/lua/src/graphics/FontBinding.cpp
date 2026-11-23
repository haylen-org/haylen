#include "graphics/FontBinding.hpp"

#include <lua.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/core/Utf8.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/plugins/TextPlugin.hpp"
#include "haylen/text/BidiParagraph.hpp"
#include "haylen/text/BitmapFont.hpp"
#include "haylen/text/Segmenter.hpp"
#include "haylen/text/SystemFonts.hpp"

namespace haylen::lua::graphics {

std::shared_ptr<haylen::text::FontFamily> FontBinding::readFamily(lua_State* L, int index) {
    if (lua::Userdata::test<haylen::text::FontFamily>(L, index) != nullptr) {
        return lua::Userdata::checkShared<haylen::text::FontFamily>(L, index);
    }

    if (lua::Userdata::test<haylen::text::Font>(L, index) != nullptr) {
        return wrapFont(L, index);
    }

    luaL_argerror(L, index, "expected a FontFamily or a Font");
    return nullptr;
}

// A font that stands for a family keeps the family it made in its user value, so every draw with the font finds the same family with the layouts it keeps.
std::shared_ptr<haylen::text::FontFamily> FontBinding::wrapFont(lua_State* L, int index) {
    const int font = lua_absindex(L, index);
    lua::Userdata::pushField(L, font, "family");
    if (lua::Userdata::test<haylen::text::FontFamily>(L, -1) != nullptr) {
        std::shared_ptr<haylen::text::FontFamily> kept = lua::Userdata::checkShared<haylen::text::FontFamily>(L, -1);
        lua_pop(L, 1);
        return kept;
    }

    lua_pop(L, 1);
    auto family = std::make_shared<haylen::text::FontFamily>(haylen::text::FontFamily::Faces{.regular = lua::Userdata::checkShared<haylen::text::Font>(L, font), .systemFonts = getSystemFonts(L)});
    lua::Stack::push(L, family);
    lua::Userdata::setField(L, font, "family", -1);
    lua_pop(L, 1);
    return family;
}

char32_t FontBinding::readCharacter(lua_State* L, int index) {
    if (lua_type(L, index) == LUA_TNUMBER) {
        const lua_Integer codePoint = luaL_checkinteger(L, index);
        luaL_argcheck(L, codePoint >= 0 && codePoint <= 0x10FFFF, index, "expected a code point from 0 to 0x10FFFF");
        return static_cast<char32_t>(codePoint);
    }

    const std::u32string decoded = haylen::core::Utf8::decode(lua::Stack::read<std::string_view>(L, index));
    luaL_argcheck(L, decoded.size() == 1, index, "expected one character or a code point");
    return decoded.front();
}

// Creates a family with `newFontFamily({regular = font, bold = font, italic = font, boldItalic = font, mono = font, fallbacks = {font, ...}, systemFonts = true})`, which draws what none of its fonts holds with the fonts of the system unless `systemFonts` is false.
int FontBinding::newFontFamily(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kFamilyFields});
    haylen::text::FontFamily::Faces faces;
    lua::Table::readField(L, 1, "regular", faces.regular);
    lua::Table::readField(L, 1, "bold", faces.bold);
    lua::Table::readField(L, 1, "italic", faces.italic);
    lua::Table::readField(L, 1, "boldItalic", faces.boldItalic);
    lua::Table::readField(L, 1, "mono", faces.mono);
    lua::Table::readField(L, 1, "fallbacks", faces.fallbacks);
    bool system = true;
    lua::Table::readField(L, 1, "systemFonts", system);

    if (system) {
        faces.systemFonts = getSystemFonts(L);
    }

    lua::Stack::push(L, std::make_shared<haylen::text::FontFamily>(std::move(faces)));
    return 1;
}

// Returns the names of the faces of the system that families loaded so far, in the order they arrived.
int FontBinding::systemFonts(lua_State* L) {
    const std::span<const haylen::text::SystemFonts::Face> faces = getSystemFonts(L)->getFaces();
    lua_createtable(L, static_cast<int>(faces.size()), 0);
    for (std::size_t index = 0; index < faces.size(); ++index) {
        lua::Stack::push(L, faces[index].name);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index) + 1);
    }

    return 1;
}

const std::shared_ptr<haylen::text::SystemFonts>& FontBinding::getSystemFonts(lua_State* L) {
    return lua::Runtime::getEngine(L).getPlugin<haylen::plugins::TextPlugin>().getSystemFonts();
}

// Creates a BMFont from the text or binary contents of a `.fnt` file and one texture per page with `newBitmapFont(data, {page, ...})`.
int FontBinding::newBitmapFont(lua_State* L) {
    const std::string_view data = lua::Stack::read<std::string_view>(L, 1);
    std::vector<haylen::graphics::Texture> pages = lua::Stack::read<std::vector<haylen::graphics::Texture>>(L, 2);
    const haylen::text::BitmapFont::Description description = haylen::text::BitmapFont::parse({reinterpret_cast<const std::uint8_t*>(data.data()), data.size()});
    lua::Stack::push(L, std::static_pointer_cast<haylen::text::Font>(std::make_shared<haylen::text::BitmapFont>(description, std::move(pages))));
    return 1;
}

// Creates a font from an image of equal cells with `newGridFont(texture, {characters = 'ABC', cellWidth = 8, cellHeight = 8, spacing, margin, advance, lineHeight, baseline})`.
int FontBinding::newGridFont(lua_State* L) {
    const haylen::graphics::Texture texture = lua::Stack::read<haylen::graphics::Texture>(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    lua::Table::checkFields(L, 2, {kGridFields});
    haylen::text::BitmapFont::Grid grid;
    lua::Table::readField(L, 2, "characters", grid.characters);
    lua::Table::readField(L, 2, "cellWidth", grid.cellWidth);
    lua::Table::readField(L, 2, "cellHeight", grid.cellHeight);
    lua::Table::readField(L, 2, "spacing", grid.spacing);
    lua::Table::readField(L, 2, "margin", grid.margin);
    lua::Table::readField(L, 2, "advance", grid.advance);
    lua::Table::readField(L, 2, "lineHeight", grid.lineHeight);
    lua::Table::readField(L, 2, "baseline", grid.baseline);
    lua::Stack::push(L, std::static_pointer_cast<haylen::text::Font>(std::make_shared<haylen::text::BitmapFont>(haylen::text::BitmapFont::describeGrid(grid, texture.getSize()), std::vector<haylen::graphics::Texture>{texture})));
    return 1;
}

int FontBinding::measure(lua_State* L) {
    const haylen::math::Vec2 size = lua::Userdata::check<haylen::text::Font>(L, 1).measure(lua::Stack::read<std::string_view>(L, 2), lua::TypeConverter::readTextStyle(L, 3));
    lua::Stack::push(L, size.x);
    lua::Stack::push(L, size.y);
    return 2;
}

// Pushes the glyph quads of a layout in visual order with the anchor of the style applied, each with the font that draws it, and the size and line count of the block. The font of every look is pushed once below the result, which the quads of the look share.
int FontBinding::pushLayout(lua_State* L, const haylen::text::Layout& laid, const haylen::text::Style& style, const std::function<std::shared_ptr<haylen::text::Font>(const haylen::text::Font*)>& fontOf) {
    const haylen::math::Vec2 anchorOffset = laid.size * style.anchor;
    const int fonts = lua_gettop(L) + 1;
    luaL_checkstack(L, static_cast<int>(laid.looks.size()) + 4, "too many fonts in one layout");

    for (const haylen::text::Layout::Look& look : laid.looks) {
        lua::Stack::push(L, fontOf(look.font));
    }

    lua_createtable(L, 0, 3);
    lua_createtable(L, static_cast<int>(laid.glyphs.size()), 0);
    for (std::size_t index = 0; index < laid.glyphs.size(); ++index) {
        const haylen::text::Layout::Glyph& glyph = laid.glyphs[index];
        lua_createtable(L, 0, 6);
        lua::Stack::push(L, glyph.position - anchorOffset);
        lua_setfield(L, -2, "position");
        lua::Stack::push(L, glyph.size);
        lua_setfield(L, -2, "size");
        lua::Stack::push(L, glyph.source);
        lua_setfield(L, -2, "source");
        lua::Stack::push(L, static_cast<int>(glyph.page) + 1);
        lua_setfield(L, -2, "page");
        lua_pushvalue(L, fonts + static_cast<int>(glyph.look));
        lua_setfield(L, -2, "font");
        if (glyph.image.isValid()) {
            lua::Stack::push(L, glyph.image);
            lua_setfield(L, -2, "image");
        }

        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    lua_setfield(L, -2, "quads");
    lua::Stack::push(L, laid.size);
    lua_setfield(L, -2, "size");
    lua::Stack::push(L, laid.lineCount);
    lua_setfield(L, -2, "lineCount");
    lua_rotate(L, fonts, 1);
    lua_settop(L, fonts);
    return 1;
}

// Lays text out with `layout(text, style)` and returns its glyph quads, its size and its line count.
int FontBinding::layout(lua_State* L) {
    const std::shared_ptr<haylen::text::Font> font = lua::Userdata::checkShared<haylen::text::Font>(L, 1);
    const haylen::text::Style style = lua::TypeConverter::readTextStyle(L, 3);
    return pushLayout(L, *font->layout(lua::Stack::read<std::string_view>(L, 2), style), style, [&font](const haylen::text::Font*) { return font; });
}

float FontBinding::lineHeight(haylen::text::Font& font, float size) {
    return font.getLineHeight(size);
}

float FontBinding::ascent(haylen::text::Font& font, float size) {
    return font.getAscent(size);
}

float FontBinding::capHeight(haylen::text::Font& font, float size) {
    return font.getCapHeight(size);
}

float FontBinding::toDistance(haylen::text::Font& font, float pixels, float size) {
    return font.toDistance(pixels, size);
}

int FontBinding::hasGlyph(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::text::Font>(L, 1).hasGlyph(readCharacter(L, 2)));
    return 1;
}

int FontBinding::hasColorGlyph(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::text::Font>(L, 1).hasColorGlyph(readCharacter(L, 2)));
    return 1;
}

// Shapes a whole text as one run in the script of its first letter and the language of the options, reading in the direction of the options or of its first paragraph.
std::vector<haylen::text::Font::ShapedGlyph> FontBinding::shapeText(haylen::text::Font& font, std::u32string_view text, haylen::text::Direction direction, std::string_view language) {
    std::vector<haylen::text::Font::ShapedGlyph> shaped;
    if (text.empty()) {
        return shaped;
    }

    const std::vector<std::uint32_t> scripts = haylen::text::Segmenter::getScripts(text);
    const auto firstParagraph = static_cast<std::size_t>(std::ranges::find_if(text, &haylen::text::Segmenter::isParagraphSeparator) - text.begin());
    const bool rightToLeft = haylen::text::BidiParagraph(text.substr(0, firstParagraph), direction).isRightToLeft();
    font.shape({.text = text, .begin = 0, .end = text.size(), .script = scripts.front(), .language = language, .rightToLeft = rightToLeft}, shaped);
    return shaped;
}

// Pushes a glyph at the native size as `{index, source, offset, advance, page, visible, ink, image}`, where only a color glyph has an image.
int FontBinding::pushGlyph(lua_State* L, haylen::text::Font& font, std::uint32_t index) {
    const haylen::text::Font::Glyph& found = font.getGlyph(index);
    lua_createtable(L, 0, 8);
    lua::Stack::push(L, index);
    lua_setfield(L, -2, "index");
    lua::Stack::push(L, found.source);
    lua_setfield(L, -2, "source");
    lua::Stack::push(L, found.offset);
    lua_setfield(L, -2, "offset");
    lua::Stack::push(L, found.advance);
    lua_setfield(L, -2, "advance");
    lua::Stack::push(L, static_cast<int>(found.page) + 1);
    lua_setfield(L, -2, "page");
    lua::Stack::push(L, found.visible);
    lua_setfield(L, -2, "visible");
    lua::Stack::push(L, found.ink);
    lua_setfield(L, -2, "ink");
    if (found.image.isValid()) {
        lua::Stack::push(L, found.image);
        lua_setfield(L, -2, "image");
    }

    return 1;
}

// Returns the glyph a character shapes to on its own.
int FontBinding::glyph(lua_State* L) {
    haylen::text::Font& font = lua::Userdata::check<haylen::text::Font>(L, 1);
    const char32_t character = readCharacter(L, 2);
    const std::vector<haylen::text::Font::ShapedGlyph> shaped = shapeText(font, std::u32string_view(&character, 1), haylen::text::Direction::Auto, {});
    return pushGlyph(L, font, shaped.empty() ? 0 : shaped.front().index);
}

// Returns the glyph with an index that `font:shape` gave, which is its code point in a bitmap font.
int FontBinding::glyphByIndex(lua_State* L) {
    haylen::text::Font& font = lua::Userdata::check<haylen::text::Font>(L, 1);
    const lua_Integer index = luaL_checkinteger(L, 2);
    luaL_argcheck(L, index >= 0 && index <= std::numeric_limits<std::uint32_t>::max(), 2, "expected a glyph index");
    return pushGlyph(L, font, static_cast<std::uint32_t>(index));
}

// Shapes text with `shape(text, {size, direction, language})` and returns its glyphs in visual order as `{index, cluster, advance, offset}`, where `cluster` counts code points from 1 and lengths are pixels at the size, the native size by default.
int FontBinding::shape(lua_State* L) {
    haylen::text::Font& font = lua::Userdata::check<haylen::text::Font>(L, 1);
    const std::u32string text = haylen::core::Utf8::decode(lua::Stack::read<std::string_view>(L, 2));
    float size = font.getNativeSize();
    haylen::text::Direction direction = haylen::text::Direction::Auto;
    std::string language;
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua::Table::checkFields(L, 3, {kShapeFields});
        lua::Table::readField(L, 3, "size", size);
        lua::Table::readField(L, 3, "direction", direction);
        lua::Table::readField(L, 3, "language", language);
    }

    const float factor = size / font.getNativeSize();
    const std::vector<haylen::text::Font::ShapedGlyph> shaped = shapeText(font, text, direction, language);
    lua_createtable(L, static_cast<int>(shaped.size()), 0);
    for (std::size_t index = 0; index < shaped.size(); ++index) {
        lua_createtable(L, 0, 4);
        lua::Stack::push(L, shaped[index].index);
        lua_setfield(L, -2, "index");
        lua::Stack::push(L, shaped[index].cluster + 1);
        lua_setfield(L, -2, "cluster");
        lua::Stack::push(L, shaped[index].advance * factor);
        lua_setfield(L, -2, "advance");
        lua::Stack::push(L, shaped[index].offset * factor);
        lua_setfield(L, -2, "offset");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

int FontBinding::page(lua_State* L) {
    haylen::text::Font& font = lua::Userdata::check<haylen::text::Font>(L, 1);
    const lua_Integer index = luaL_checkinteger(L, 2);
    luaL_argcheck(L, index >= 1 && static_cast<std::size_t>(index) <= font.getPageCount(), 2, "page out of range");
    font.sync();
    lua::Stack::push(L, font.getPage(static_cast<std::size_t>(index - 1)));
    return 1;
}

int FontBinding::nativeSize(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::text::Font>(L, 1).getNativeSize());
    return 1;
}

int FontBinding::distanceField(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::text::Font>(L, 1).isDistanceField());
    return 1;
}

int FontBinding::pageCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::text::Font>(L, 1).getPageCount());
    return 1;
}

std::shared_ptr<haylen::text::Font> FontBinding::findFace(const haylen::text::FontFamily& family, const haylen::text::Font* font) {
    const haylen::text::FontFamily::Faces& faces = family.getFaces();

    for (const std::shared_ptr<haylen::text::Font>& face : {faces.regular, faces.bold, faces.italic, faces.boldItalic, faces.mono}) {
        if (face.get() == font) {
            return face;
        }
    }

    for (const std::shared_ptr<haylen::text::Font>& fallback : faces.fallbacks) {
        if (fallback.get() == font) {
            return fallback;
        }
    }

    if (!faces.systemFonts) {
        return nullptr;
    }

    for (const haylen::text::SystemFonts::Face& system : faces.systemFonts->getFaces()) {
        if (system.font.get() == font) {
            return system.font;
        }
    }

    return nullptr;
}

int FontBinding::pushSelection(lua_State* L, const haylen::text::FontFamily& family, const haylen::text::FontFamily::Selection& selection) {
    lua::Stack::push(L, findFace(family, selection.font));
    lua::Stack::push(L, selection.syntheticBold);
    lua::Stack::push(L, selection.syntheticItalic);
    lua::Stack::push(L, selection.waiting);
    return 4;
}

int FontBinding::familySystemFonts(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::text::FontFamily>(L, 1).getFaces().systemFonts != nullptr);
    return 1;
}

// Reads the faces as properties named like the fields of `newFontFamily`, which are `nil` for the faces the family lacks.
int FontBinding::familyFace(lua_State* L) {
    const haylen::text::FontFamily::Faces& faces = lua::Userdata::check<haylen::text::FontFamily>(L, 1).getFaces();
    const std::string_view key = lua::Stack::read<std::string_view>(L, 2);
    const std::shared_ptr<haylen::text::Font>* face = &faces.regular;
    if (key == "bold") {
        face = &faces.bold;
    } else if (key == "italic") {
        face = &faces.italic;
    } else if (key == "boldItalic") {
        face = &faces.boldItalic;
    } else if (key == "mono") {
        face = &faces.mono;
    }

    lua::Stack::push(L, *face);
    return 1;
}

int FontBinding::familyFallbacks(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::text::FontFamily>(L, 1).getFaces().fallbacks);
    return 1;
}

// Picks the face of a style with `family:select({bold, italic, mono})` and returns it with whether bold and italic are synthesized.
int FontBinding::familySelect(lua_State* L) {
    const haylen::text::FontFamily& family = lua::Userdata::check<haylen::text::FontFamily>(L, 1);
    bool bold = false;
    bool italic = false;
    bool mono = false;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kStyleFields});
        lua::Table::readField(L, 2, "bold", bold);
        lua::Table::readField(L, 2, "italic", italic);
        lua::Table::readField(L, 2, "mono", mono);
    }

    return pushSelection(L, family, family.select(bold, italic, mono));
}

// Picks the font that draws a character in a style with `family:resolve(character, {bold, italic, mono})`, which is a fallback when the face lacks the glyph. The character may be a letter with its marks, given as one string.
int FontBinding::familyResolve(lua_State* L) {
    const haylen::text::FontFamily& family = lua::Userdata::check<haylen::text::FontFamily>(L, 1);
    const std::u32string character = lua_type(L, 2) == LUA_TNUMBER ? std::u32string(1, readCharacter(L, 2)) : haylen::core::Utf8::decode(lua::Stack::read<std::string_view>(L, 2));
    luaL_argcheck(L, !character.empty(), 2, "expected a character");
    bool bold = false;
    bool italic = false;
    bool mono = false;
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua::Table::checkFields(L, 3, {kStyleFields});
        lua::Table::readField(L, 3, "bold", bold);
        lua::Table::readField(L, 3, "italic", italic);
        lua::Table::readField(L, 3, "mono", mono);
    }

    return pushSelection(L, family, family.resolve(family.select(bold, italic, mono), character, bold, italic));
}

int FontBinding::familyMeasure(lua_State* L) {
    const haylen::math::Vec2 size = lua::Userdata::check<haylen::text::FontFamily>(L, 1).measure(lua::Stack::read<std::string_view>(L, 2), lua::TypeConverter::readTextStyle(L, 3));
    lua::Stack::push(L, size.x);
    lua::Stack::push(L, size.y);
    return 2;
}

// Lays text out with `family:layout(text, style)` like a font does, where each quad names the face or fallback that draws it.
int FontBinding::familyLayout(lua_State* L) {
    const std::shared_ptr<haylen::text::FontFamily> family = lua::Userdata::checkShared<haylen::text::FontFamily>(L, 1);
    const haylen::text::Style style = lua::TypeConverter::readTextStyle(L, 3);
    return pushLayout(L, *family->layout(lua::Stack::read<std::string_view>(L, 2), style), style, [&family](const haylen::text::Font* font) { return findFace(*family, font); });
}

void FontBinding::install(lua_State* L) {
    // clang-format off
    lua::ClassBuilder<haylen::text::Font>(L)
        .function("measure", &lua::Binding::native<&measure>)
        .function("layout", &lua::Binding::native<&layout>)
        .function("lineHeight", &lua::Binding::function<&lineHeight>)
        .function("ascent", &lua::Binding::function<&ascent>)
        .function("capHeight", &lua::Binding::function<&capHeight>)
        .function("toDistance", &lua::Binding::function<&toDistance>)
        .function("hasGlyph", &lua::Binding::native<&hasGlyph>)
        .function("hasColorGlyph", &lua::Binding::native<&hasColorGlyph>)
        .function("glyph", &lua::Binding::native<&glyph>)
        .function("glyphByIndex", &lua::Binding::native<&glyphByIndex>)
        .function("shape", &lua::Binding::native<&shape>)
        .function("page", &lua::Binding::native<&page>)
        .property("nativeSize", &nativeSize)
        .property("distanceField", &distanceField)
        .property("pageCount", &pageCount)
        .meta("__eq", &lua::Userdata::equal<haylen::text::Font>)
        .install();

    lua::ClassBuilder<haylen::text::FontFamily>(L)
        .property("regular", &familyFace)
        .property("bold", &familyFace)
        .property("italic", &familyFace)
        .property("boldItalic", &familyFace)
        .property("mono", &familyFace)
        .property("fallbacks", &familyFallbacks)
        .property("systemFonts", &familySystemFonts)
        .function("select", &lua::Binding::native<&familySelect>)
        .function("resolve", &lua::Binding::native<&familyResolve>)
        .function("measure", &lua::Binding::native<&familyMeasure>)
        .function("layout", &lua::Binding::native<&familyLayout>)
        .meta("__eq", &lua::Userdata::equal<haylen::text::FontFamily>)
        .install();
    // clang-format on
}

} // namespace haylen::lua::graphics
