#pragma once

#include <array>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/lua/Table.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/plugins/TextPlugin.hpp"
#include "haylen/text/Effect.hpp"
#include "haylen/text/Layout.hpp"
#include "haylen/text/RichText.hpp"
#include "haylen/text/RichTextOptions.hpp"
#include "haylen/text/RichTextRegistry.hpp"
#include "runtime/Owners.hpp"

struct lua_State;

namespace haylen::lua {

template <> struct Type<haylen::text::RichText> {
    static constexpr const char* name = "haylen.RichText";
    using Storage = std::shared_ptr<haylen::text::RichText>;
};

} // namespace haylen::lua

namespace haylen::lua::graphics2d {

// Installs the `RichText` class of `haylen.graphics2d`, and the functions that make, draw and measure rich text and register the effects and icons its markup names.
class RichTextBinding final {
  public:
    static void install(lua_State* L);

    static int newRichText(lua_State* L);
    static int drawRichText(lua_State* L);
    static int measureRichText(lua_State* L);
    static int registerTextEffect(lua_State* L);
    static int registerTextIcon(lua_State* L);
    static int textEffectNames(lua_State* L);

    // Reads the options of rich text, where a missing family takes the default font and `fonts` maps the names of `[font]` tags to families or fonts, which also land in `fontNames` in order of name when it is given.
    [[nodiscard]] static haylen::text::RichTextOptions readOptions(lua_State* L, int index, std::initializer_list<lua::Table::FieldNames> extraFields = {}, std::vector<haylen::plugins::TextPlugin::FontName>* fontNames = nullptr);
    [[nodiscard]] static const std::shared_ptr<haylen::text::RichTextRegistry>& getRegistry(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 18> kOptionFields{"family", "size", "bold", "italic", "color", "maxWidth", "balance", "align", "direction", "language", "lineSpacing", "lineHeight", "scale", "revealSpeed", "underlineLinks", "ligatures", "code", "fonts"};
    static constexpr std::array<std::string_view, 3> kIconFields{"source", "width", "height"};
    static constexpr std::array<std::string_view, 2> kDrawFields{"scale", "tint"};
    static constexpr std::array<std::string_view, 1> kTintFields{"tint"};

    // The names of the kinds of the boxes of a layout, in the order of `text::Layout::Box::Kind`.
    static constexpr std::array<std::string_view, 6> kBoxKindNames{"background", "underline", "strike", "rule", "cellBackground", "cellBorder"};

    // Returns the rich text that the text plugin keeps for the markup of an immediate draw or measure with the options at the index.
    [[nodiscard]] static haylen::text::RichText& getDrawnText(lua_State* L, int options);

    // Reads the tint of a rich text draw, which multiplies every color of the text.
    [[nodiscard]] static haylen::math::Color readTint(lua_State* L, int index);
    static void readFonts(lua_State* L, std::vector<haylen::plugins::TextPlugin::FontName>& fontNames);

    // Runs a Lua effect on one glyph, which reads and writes the fields of a glyph table and reads the attributes of its tag.
    static void runEffect(lua_State* main, const lua::Owners::Function& function, haylen::text::Effect::Glyph& glyph, const haylen::text::Effect::Parameters& parameters);
    static void pushEffectGlyph(lua_State* L, const haylen::text::Effect::Glyph& glyph);
    static void pushEffectAttributes(lua_State* L, const haylen::text::Effect::Parameters& parameters);

    static void pushGlyphs(lua_State* L, const haylen::text::Layout& frame);
    static void pushBoxes(lua_State* L, const haylen::text::Layout& frame);
    static void pushImages(lua_State* L, const haylen::text::Layout& frame);
    static void pushAreas(lua_State* L, const std::vector<haylen::text::Layout::Area>& areas, const std::vector<std::string>& values, const char* key);
    static void pushCharacters(lua_State* L, const haylen::text::Layout& frame);
    static void pushLines(lua_State* L, const haylen::text::Layout& frame);

    static int update(lua_State* L);
    static int draw(lua_State* L);
    static int size(lua_State* L);
    static int linkAt(lua_State* L);
    static int hintAt(lua_State* L);
    static int setVisibleCharacters(lua_State* L);
    static int frame(lua_State* L);

    static int getMarkup(lua_State* L);
    static int setMarkup(lua_State* L);
    static int getMaxWidth(lua_State* L);
    static int setMaxWidth(lua_State* L);
    static int getScale(lua_State* L);
    static int setScale(lua_State* L);
    template <auto Option> static int getOption(lua_State* L);
    template <auto Option> static int setOption(lua_State* L);
    static int setFamily(lua_State* L);
    static int getVisibleCharacters(lua_State* L);
    static int getVisibleRatio(lua_State* L);
    static int setVisibleRatio(lua_State* L);
    static int characterCount(lua_State* L);
    static int revealing(lua_State* L);
    static int time(lua_State* L);
};

} // namespace haylen::lua::graphics2d
