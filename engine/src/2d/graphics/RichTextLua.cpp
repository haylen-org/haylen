#include "2d/graphics/RichTextLua.hpp"

#include <lua.hpp>

#include <map>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "graphics/FontLua.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/core/Utf8.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Reference.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/plugins/TextPlugin.hpp"

namespace haylen::graphics2d {

const std::shared_ptr<text::RichTextRegistry>& RichTextLua::getRegistry(lua_State* L) {
    return lua::Runtime::getEngine(L).getPlugin<plugins::TextPlugin>().getRegistry();
}

text::RichTextOptions RichTextLua::readOptions(lua_State* L, int index, std::initializer_list<lua::Table::FieldNames> extraFields) {
    core::Engine& engine = lua::Runtime::getEngine(L);
    plugins::TextPlugin& plugin = engine.getPlugin<plugins::TextPlugin>();
    text::RichTextOptions options{.family = plugin.getRegistry()->getDefaultFamily(), .images = [&engine, &plugin](std::string_view path) { return plugin.getImage(engine, path); }};
    if (lua_isnoneornil(L, index)) {
        return options;
    }
    luaL_checktype(L, index, LUA_TTABLE);
    const int table = lua_absindex(L, index);
    std::vector<lua::Table::FieldNames> allowed{kOptionFields};
    allowed.insert(allowed.end(), extraFields.begin(), extraFields.end());
    lua::Table::checkFields(L, table, allowed);

    if (lua_getfield(L, table, "family") != LUA_TNIL) {
        options.family = graphics::FontLua::readFamily(L, lua_gettop(L));
    }
    lua_pop(L, 1);
    lua::Table::readField(L, table, "size", options.size);
    lua::Table::readField(L, table, "bold", options.bold);
    lua::Table::readField(L, table, "italic", options.italic);
    lua::Table::readField(L, table, "color", options.color);
    lua::Table::readField(L, table, "maxWidth", options.maxWidth);
    lua::Table::readField(L, table, "align", options.align);
    lua::Table::readField(L, table, "direction", options.direction);
    lua::Table::readField(L, table, "language", options.language);
    lua::Table::readField(L, table, "lineSpacing", options.lineSpacing);
    lua::Table::readField(L, table, "scale", options.scale);
    lua::Table::readField(L, table, "revealSpeed", options.revealSpeed);
    lua::Table::readField(L, table, "underlineLinks", options.underlineLinks);

    if (lua_getfield(L, table, "fonts") != LUA_TNIL) {
        luaL_checktype(L, -1, LUA_TTABLE);
        std::map<std::string, std::shared_ptr<text::FontFamily>, std::less<>> fonts;
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            if (lua_type(L, -2) != LUA_TSTRING) {
                luaL_error(L, "The \"fonts\" option maps font names to families, so its keys must be strings.");
            }
            fonts.insert_or_assign(lua::Stack::read<std::string>(L, -2), graphics::FontLua::readFamily(L, lua_gettop(L)));
            lua_pop(L, 1);
        }
        // clang-format off
        options.fonts = [fonts = std::move(fonts)](std::string_view name) -> std::shared_ptr<text::FontFamily> {
            const auto found = fonts.find(name);
            return found != fonts.end() ? found->second : nullptr;
        };
        // clang-format on
    }
    lua_pop(L, 1);
    return options;
}

// Creates rich text with `newRichText(markup, {family, size, bold, italic, color, maxWidth, align, direction, language, lineSpacing, scale, revealSpeed, underlineLinks, fonts})`.
int RichTextLua::newRichText(lua_State* L) {
    std::string markup = lua::Stack::read<std::string>(L, 1);
    lua::Stack::push(L, std::make_shared<text::RichText>(std::move(markup), readOptions(L, 2), getRegistry(L)));
    return 1;
}

math::Color RichTextLua::readTint(lua_State* L, int index) {
    math::Color tint = math::Color::white();
    if (!lua_isnoneornil(L, index)) {
        lua::Table::readField(L, index, "tint", tint);
    }
    return tint;
}

// Draws markup once with `drawRichText(markup, x, y, options)`, where the options also take the tint and the draw order, and effects follow the time the app has run.
int RichTextLua::drawRichText(lua_State* L) {
    core::Engine& engine = lua::Runtime::getEngine(L);
    text::RichText richText(lua::Stack::read<std::string>(L, 1), readOptions(L, 4, {lua::TypeConverter::kDrawOrderFields, kTintFields}), getRegistry(L));
    richText.update(static_cast<float>(engine.getClock().getElapsed()));
    engine.getRenderer2D().drawRichText(richText, {lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}, lua::TypeConverter::readDrawOrder(L, 4, {kOptionFields, kTintFields}), {1.0F, 1.0F}, readTint(L, 4));
    return 0;
}

int RichTextLua::measureRichText(lua_State* L) {
    text::RichText richText(lua::Stack::read<std::string>(L, 1), readOptions(L, 2, {lua::TypeConverter::kDrawOrderFields, kTintFields}), getRegistry(L));
    const math::Vec2 size = richText.getSize();
    lua::Stack::push(L, size.x);
    lua::Stack::push(L, size.y);
    return 2;
}

// The glyph table holds `index` and `character` counted from 1, `char`, `codePoint`, `x` and `y` on the baseline, `time`, `offsetX`, `offsetY`, `color` and `visible`, and the attributes of the tag come as the second argument, with numbers as numbers.
void RichTextLua::runEffect(const lua::Reference& function, text::Effect::Glyph& glyph, const text::Effect::Parameters& parameters) {
    // clang-format off
    lua::Runtime::protectedRun(function.getState(), [&](lua_State* L) {
        function.push(L);
        lua_createtable(L, 0, 12);
        const int table = lua_gettop(L);
        const std::pair<const char*, double> numbers[] = {{"x", glyph.position.x}, {"y", glyph.position.y}, {"time", glyph.time}, {"offsetX", glyph.offset.x}, {"offsetY", glyph.offset.y}};
        for (const auto& [name, value] : numbers) {
            lua_pushnumber(L, value);
            lua_setfield(L, table, name);
        }
        lua_pushinteger(L, static_cast<lua_Integer>(glyph.index + 1));
        lua_setfield(L, table, "index");
        lua_pushinteger(L, static_cast<lua_Integer>(glyph.character + 1));
        lua_setfield(L, table, "character");
        std::string character;
        core::Utf8::append(character, glyph.codePoint);
        lua::Stack::push(L, character);
        lua_setfield(L, table, "char");
        lua_pushinteger(L, static_cast<lua_Integer>(glyph.codePoint));
        lua_setfield(L, table, "codePoint");
        lua::Stack::push(L, glyph.color);
        lua_setfield(L, table, "color");
        lua::Stack::push(L, glyph.visible);
        lua_setfield(L, table, "visible");

        lua_createtable(L, 0, static_cast<int>(parameters.getValues().size()));
        for (const auto& [key, value] : parameters.getValues()) {
            if (lua_stringtonumber(L, value.c_str()) == 0) {
                lua::Stack::push(L, value);
            }
            lua_setfield(L, -2, key.c_str());
        }

        lua_pushvalue(L, table);
        lua_insert(L, 1);
        lua_call(L, 2, 0);
        lua::Table::readField(L, 1, "offsetX", glyph.offset.x);
        lua::Table::readField(L, 1, "offsetY", glyph.offset.y);
        lua::Table::readField(L, 1, "color", glyph.color);
        lua::Table::readField(L, 1, "visible", glyph.visible);
    });
    // clang-format on
}

// Registers a Lua effect with `registerTextEffect(name, function(glyph, attributes) ... end)`, which runs for every glyph inside `[name]` each frame.
int RichTextLua::registerTextEffect(lua_State* L) {
    std::string name = lua::Stack::read<std::string>(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    auto function = std::make_shared<lua::Reference>(L, 2);
    getRegistry(L)->registerEffect(std::move(name), [function](text::Effect::Glyph& glyph, const text::Effect::Parameters& parameters) { runEffect(*function, glyph, parameters); });
    return 0;
}

// Registers an icon for `[icon=name]` with `registerTextIcon(name, texture, {source, width, height})`.
int RichTextLua::registerTextIcon(lua_State* L) {
    std::string name = lua::Stack::read<std::string>(L, 1);
    text::RichTextRegistry::Icon icon{.texture = lua::Stack::read<graphics::Texture>(L, 2)};
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua::Table::checkFields(L, 3, {kIconFields});
        lua::Table::readField(L, 3, "source", icon.source);
        lua::Table::readField(L, 3, "width", icon.size.x);
        lua::Table::readField(L, 3, "height", icon.size.y);
    }
    getRegistry(L)->registerIcon(std::move(name), std::move(icon));
    return 0;
}

int RichTextLua::textEffectNames(lua_State* L) {
    lua::Stack::push(L, getRegistry(L)->getEffectNames());
    return 1;
}

int RichTextLua::update(lua_State* L) {
    lua::Userdata::check<text::RichText>(L, 1).update(lua::Stack::read<float>(L, 2));
    return 0;
}

// Draws the text with `text:draw(x, y, options)`, where the options take the scale of the block from its top-left corner, the tint of every color and the draw order keys.
int RichTextLua::draw(lua_State* L) {
    const DrawOrder order = lua::TypeConverter::readDrawOrder(L, 4, {kDrawFields});
    math::Vec2 scale{1.0F, 1.0F};
    if (!lua_isnoneornil(L, 4)) {
        lua::Table::readField(L, 4, "scale", scale);
    }
    lua::Runtime::getEngine(L).getRenderer2D().drawRichText(lua::Userdata::check<text::RichText>(L, 1), {lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}, order, scale, readTint(L, 4));
    return 0;
}

// Returns the size of the block, or with `text:size(maxWidth)` the size the block takes at another wrap width without changing the text, which is how a container measures text before placing it.
int RichTextLua::size(lua_State* L) {
    text::RichText& richText = lua::Userdata::check<text::RichText>(L, 1);
    const math::Vec2 measured = lua_isnoneornil(L, 2) ? richText.getSize() : richText.getLayout(lua::Stack::read<float>(L, 2)).size;
    lua::Stack::push(L, measured.x);
    lua::Stack::push(L, measured.y);
    return 2;
}

int RichTextLua::linkAt(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<text::RichText>(L, 1).getLinkAt({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}));
    return 1;
}

int RichTextLua::hintAt(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<text::RichText>(L, 1).getHintAt({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}));
    return 1;
}

// A negative count shows every character.
int RichTextLua::setVisibleCharacters(lua_State* L) {
    text::RichText& richText = lua::Userdata::check<text::RichText>(L, 1);
    const lua_Integer count = luaL_checkinteger(L, lua_gettop(L));
    richText.setVisibleCharacters(count < 0 ? richText.getCharacterCount() : static_cast<std::size_t>(count));
    return 0;
}

// Returns the layout of this moment as `{size, lineCount, glyphs, boxes, images, links, hints, characters, lines}`, the way the text draws now.
int RichTextLua::frame(lua_State* L) {
    text::RichText& richText = lua::Userdata::check<text::RichText>(L, 1);
    const text::Layout& frame = richText.getFrame();
    lua_createtable(L, 0, 9);
    lua::Stack::push(L, frame.size);
    lua_setfield(L, -2, "size");
    lua::Stack::push(L, frame.lineCount);
    lua_setfield(L, -2, "lineCount");

    lua_createtable(L, static_cast<int>(frame.glyphs.size()), 0);
    for (std::size_t index = 0; index < frame.glyphs.size(); ++index) {
        const text::Layout::Glyph& glyph = frame.glyphs[index];
        const text::Layout::Look& look = frame.looks[glyph.look];
        std::string character;
        core::Utf8::append(character, glyph.codePoint);
        lua_createtable(L, 0, 11);
        lua::Stack::push(L, character);
        lua_setfield(L, -2, "char");
        lua::Stack::push(L, glyph.index);
        lua_setfield(L, -2, "index");
        lua::Stack::push(L, glyph.character + 1);
        lua_setfield(L, -2, "character");
        lua::Stack::push(L, math::Rect{glyph.position.x, glyph.position.y, glyph.size.x, glyph.size.y});
        lua_setfield(L, -2, "rect");
        lua::Stack::push(L, glyph.baseline);
        lua_setfield(L, -2, "baseline");
        lua::Stack::push(L, glyph.color);
        lua_setfield(L, -2, "color");
        lua::Stack::push(L, glyph.visible);
        lua_setfield(L, -2, "visible");
        lua::Stack::push(L, look.size);
        lua_setfield(L, -2, "size");
        lua::Stack::push(L, look.weight > 0.0F || look.emboldenOffset > 0.0F);
        lua_setfield(L, -2, "syntheticBold");
        lua::Stack::push(L, look.skew != 0.0F);
        lua_setfield(L, -2, "syntheticItalic");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    lua_setfield(L, -2, "glyphs");

    constexpr std::array<std::string_view, 6> kKinds{"background", "underline", "strike", "rule", "cellBackground", "cellBorder"};
    lua_createtable(L, static_cast<int>(frame.boxes.size()), 0);
    for (std::size_t index = 0; index < frame.boxes.size(); ++index) {
        const text::Layout::Box& box = frame.boxes[index];
        lua_createtable(L, 0, 4);
        lua::Stack::push(L, kKinds[static_cast<std::size_t>(box.kind)]);
        lua_setfield(L, -2, "kind");
        lua::Stack::push(L, box.rect);
        lua_setfield(L, -2, "rect");
        lua::Stack::push(L, box.color);
        lua_setfield(L, -2, "color");
        lua::Stack::push(L, box.visible);
        lua_setfield(L, -2, "visible");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    lua_setfield(L, -2, "boxes");

    lua_createtable(L, static_cast<int>(frame.images.size()), 0);
    for (std::size_t index = 0; index < frame.images.size(); ++index) {
        lua_createtable(L, 0, 3);
        lua::Stack::push(L, frame.images[index].rect);
        lua_setfield(L, -2, "rect");
        lua::Stack::push(L, frame.images[index].texture);
        lua_setfield(L, -2, "texture");
        lua::Stack::push(L, frame.images[index].visible);
        lua_setfield(L, -2, "visible");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    lua_setfield(L, -2, "images");

    // clang-format off
    const auto pushAreas = [&](const std::vector<text::Layout::Area>& areas, const std::vector<std::string>& values, const char* key) {
        lua_createtable(L, static_cast<int>(areas.size()), 0);
        for (std::size_t index = 0; index < areas.size(); ++index) {
            lua_createtable(L, 0, 2);
            lua::Stack::push(L, areas[index].rect);
            lua_setfield(L, -2, "rect");
            lua::Stack::push(L, values[areas[index].index]);
            lua_setfield(L, -2, key);
            lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
        }
    };
    // clang-format on
    pushAreas(frame.links, richText.getDocument().links, "link");
    lua_setfield(L, -2, "links");
    pushAreas(frame.hints, richText.getDocument().hints, "hint");
    lua_setfield(L, -2, "hints");

    // Characters come in reading order with the code points of the text without markup they draw, counted from 1.
    lua_createtable(L, static_cast<int>(frame.characters.size()), 0);
    for (std::size_t index = 0; index < frame.characters.size(); ++index) {
        const text::Layout::Character& character = frame.characters[index];
        lua_createtable(L, 0, 4);
        lua::Stack::push(L, character.box);
        lua_setfield(L, -2, "rect");
        lua::Stack::push(L, character.begin + 1);
        lua_setfield(L, -2, "first");
        lua::Stack::push(L, character.end);
        lua_setfield(L, -2, "last");
        lua::Stack::push(L, character.rightToLeft);
        lua_setfield(L, -2, "rightToLeft");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    lua_setfield(L, -2, "characters");

    lua_createtable(L, static_cast<int>(frame.lines.size()), 0);
    for (std::size_t index = 0; index < frame.lines.size(); ++index) {
        const text::Layout::Line& line = frame.lines[index];
        lua_createtable(L, 0, 3);
        lua::Stack::push(L, line.box);
        lua_setfield(L, -2, "rect");
        lua::Stack::push(L, line.baseline);
        lua_setfield(L, -2, "baseline");
        lua::Stack::push(L, line.rightToLeft);
        lua_setfield(L, -2, "rightToLeft");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    lua_setfield(L, -2, "lines");
    return 1;
}

int RichTextLua::getMarkup(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<text::RichText>(L, 1).getMarkup());
    return 1;
}

int RichTextLua::setMarkup(lua_State* L) {
    lua::Userdata::check<text::RichText>(L, 1).setMarkup(lua::Stack::read<std::string>(L, 3));
    return 0;
}

int RichTextLua::getMaxWidth(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<text::RichText>(L, 1).getOptions().maxWidth);
    return 1;
}

int RichTextLua::setMaxWidth(lua_State* L) {
    lua::Userdata::check<text::RichText>(L, 1).setMaxWidth(lua::Stack::read<float>(L, 3));
    return 0;
}

int RichTextLua::getScale(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<text::RichText>(L, 1).getOptions().scale);
    return 1;
}

int RichTextLua::setScale(lua_State* L) {
    lua::Userdata::check<text::RichText>(L, 1).setScale(lua::Stack::read<float>(L, 3));
    return 0;
}

template <auto Option> int RichTextLua::getOption(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<text::RichText>(L, 1).getOptions().*Option);
    return 1;
}

// An option set as a property lays the text out again and starts its reveal over, like any change of the options.
template <auto Option> int RichTextLua::setOption(lua_State* L) {
    text::RichText& richText = lua::Userdata::check<text::RichText>(L, 1);
    text::RichTextOptions changed = richText.getOptions();
    changed.*Option = lua::Stack::read<std::remove_cvref_t<decltype(changed.*Option)>>(L, 3);
    richText.setOptions(std::move(changed));
    return 0;
}

int RichTextLua::setFamily(lua_State* L) {
    text::RichText& richText = lua::Userdata::check<text::RichText>(L, 1);
    text::RichTextOptions changed = richText.getOptions();
    changed.family = graphics::FontLua::readFamily(L, 3);
    richText.setOptions(std::move(changed));
    return 0;
}

int RichTextLua::getVisibleCharacters(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<text::RichText>(L, 1).getVisibleCharacters());
    return 1;
}

int RichTextLua::getVisibleRatio(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<text::RichText>(L, 1).getVisibleRatio());
    return 1;
}

int RichTextLua::setVisibleRatio(lua_State* L) {
    lua::Userdata::check<text::RichText>(L, 1).setVisibleRatio(lua::Stack::read<float>(L, 3));
    return 0;
}

int RichTextLua::characterCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<text::RichText>(L, 1).getCharacterCount());
    return 1;
}

int RichTextLua::revealing(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<text::RichText>(L, 1).isRevealing());
    return 1;
}

int RichTextLua::time(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<text::RichText>(L, 1).getTime());
    return 1;
}

void RichTextLua::install(lua_State* L) {
    lua::ClassBuilder<text::RichText>(L).function("update", &lua::Binding::native<&update>).function("draw", &lua::Binding::native<&draw>).function("size", &lua::Binding::native<&size>).function("linkAt", &lua::Binding::native<&linkAt>).function("hintAt", &lua::Binding::native<&hintAt>).function("setVisibleCharacters", &lua::Binding::native<&setVisibleCharacters>).function("frame", &lua::Binding::native<&frame>).property("markup", &lua::Binding::native<&getMarkup>, &lua::Binding::native<&setMarkup>).property("maxWidth", &lua::Binding::native<&getMaxWidth>, &lua::Binding::native<&setMaxWidth>).property("scale", &lua::Binding::native<&getScale>, &lua::Binding::native<&setScale>).property("family", &lua::Binding::native<&getOption<&text::RichTextOptions::family>>, &lua::Binding::native<&setFamily>).property("bold", &lua::Binding::native<&getOption<&text::RichTextOptions::bold>>, &lua::Binding::native<&setOption<&text::RichTextOptions::bold>>).property("italic", &lua::Binding::native<&getOption<&text::RichTextOptions::italic>>, &lua::Binding::native<&setOption<&text::RichTextOptions::italic>>).property("color", &lua::Binding::native<&getOption<&text::RichTextOptions::color>>, &lua::Binding::native<&setOption<&text::RichTextOptions::color>>).property("align", &lua::Binding::native<&getOption<&text::RichTextOptions::align>>, &lua::Binding::native<&setOption<&text::RichTextOptions::align>>).property("direction", &lua::Binding::native<&getOption<&text::RichTextOptions::direction>>, &lua::Binding::native<&setOption<&text::RichTextOptions::direction>>).property("language", &lua::Binding::native<&getOption<&text::RichTextOptions::language>>, &lua::Binding::native<&setOption<&text::RichTextOptions::language>>).property("lineSpacing", &lua::Binding::native<&getOption<&text::RichTextOptions::lineSpacing>>, &lua::Binding::native<&setOption<&text::RichTextOptions::lineSpacing>>).property("revealSpeed", &lua::Binding::native<&getOption<&text::RichTextOptions::revealSpeed>>, &lua::Binding::native<&setOption<&text::RichTextOptions::revealSpeed>>).property("underlineLinks", &lua::Binding::native<&getOption<&text::RichTextOptions::underlineLinks>>, &lua::Binding::native<&setOption<&text::RichTextOptions::underlineLinks>>).property("visibleCharacters", &lua::Binding::native<&getVisibleCharacters>, &lua::Binding::native<&setVisibleCharacters>).property("visibleRatio", &lua::Binding::native<&getVisibleRatio>, &lua::Binding::native<&setVisibleRatio>).property("characterCount", &lua::Binding::native<&characterCount>).property("revealing", &lua::Binding::native<&revealing>).property("time", &time).install();
}

} // namespace haylen::graphics2d
