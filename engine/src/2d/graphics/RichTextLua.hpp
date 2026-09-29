#pragma once

#include <array>
#include <initializer_list>
#include <memory>
#include <string_view>

#include "haylen/lua/Reference.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/text/Effect.hpp"
#include "haylen/text/RichText.hpp"
#include "haylen/text/RichTextOptions.hpp"
#include "haylen/text/RichTextRegistry.hpp"

struct lua_State;

namespace haylen::lua {

template <> struct Type<text::RichText> {
    static constexpr const char* name = "haylen.RichText";
    using Storage = std::shared_ptr<text::RichText>;
};

} // namespace haylen::lua

namespace haylen::graphics2d {

// Installs the RichText class of haylen.graphics2d, and the functions that make, draw and measure rich text and register the effects and icons its markup names.
class RichTextLua final {
  public:
    static void install(lua_State* L);

    static int newRichText(lua_State* L);
    static int drawRichText(lua_State* L);
    static int measureRichText(lua_State* L);
    static int registerTextEffect(lua_State* L);
    static int registerTextIcon(lua_State* L);
    static int textEffectNames(lua_State* L);

    // Reads the options of rich text, where a missing family takes the default font and fonts maps the names of [font] tags to families or fonts.
    [[nodiscard]] static text::RichTextOptions readOptions(lua_State* L, int index, std::initializer_list<lua::Table::FieldNames> extraFields = {});
    [[nodiscard]] static const std::shared_ptr<text::RichTextRegistry>& getRegistry(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 14> kOptionFields{"family", "size", "bold", "italic", "color", "maxWidth", "align", "direction", "language", "lineSpacing", "scale", "revealSpeed", "underlineLinks", "fonts"};
    static constexpr std::array<std::string_view, 3> kIconFields{"source", "width", "height"};
    static constexpr std::array<std::string_view, 2> kDrawFields{"scale", "tint"};
    static constexpr std::array<std::string_view, 1> kTintFields{"tint"};

    // Reads the tint of a rich text draw, which multiplies every color of the text.
    [[nodiscard]] static math::Color readTint(lua_State* L, int index);

    // Runs a Lua effect on one glyph, which reads and writes the fields of a glyph table and reads the attributes of its tag.
    static void runEffect(const lua::Reference& function, text::Effect::Glyph& glyph, const text::Effect::Parameters& parameters);

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

} // namespace haylen::graphics2d
