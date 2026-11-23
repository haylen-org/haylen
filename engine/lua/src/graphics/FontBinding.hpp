#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>

#include "haylen/lua/Type.hpp"
#include "haylen/text/Font.hpp"
#include "haylen/text/FontFamily.hpp"

struct lua_State;

namespace haylen::lua {

template <> struct Type<haylen::text::FontFamily> {
    static constexpr const char* name = "haylen.FontFamily";
    using Storage = std::shared_ptr<haylen::text::FontFamily>;
};

} // namespace haylen::lua

namespace haylen::lua::graphics {

// Installs the `Font` and `FontFamily` classes of `haylen.graphics` and the functions that make families and bitmap fonts.
class FontBinding final {
  public:
    static void install(lua_State* L);

    static int newFontFamily(lua_State* L);
    static int newBitmapFont(lua_State* L);
    static int newGridFont(lua_State* L);
    static int systemFonts(lua_State* L);

    // Reads a `FontFamily`, or a `Font`, which becomes the regular face of a family of its own that draws what the font lacks with the fonts of the system.
    [[nodiscard]] static std::shared_ptr<haylen::text::FontFamily> readFamily(lua_State* L, int index);

  private:
    [[nodiscard]] static std::shared_ptr<haylen::text::FontFamily> wrapFont(lua_State* L, int index);

    static constexpr std::array<std::string_view, 7> kFamilyFields{"regular", "bold", "italic", "boldItalic", "mono", "fallbacks", "systemFonts"};
    static constexpr std::array<std::string_view, 8> kGridFields{"characters", "cellWidth", "cellHeight", "spacing", "margin", "advance", "lineHeight", "baseline"};
    static constexpr std::array<std::string_view, 3> kStyleFields{"bold", "italic", "mono"};
    static constexpr std::array<std::string_view, 3> kShapeFields{"size", "direction", "language"};

    // Reads a character given as a string of one character or as a code point.
    [[nodiscard]] static char32_t readCharacter(lua_State* L, int index);
    [[nodiscard]] static std::shared_ptr<haylen::text::Font> findFace(const haylen::text::FontFamily& family, const haylen::text::Font* font);
    static int pushSelection(lua_State* L, const haylen::text::FontFamily& family, const haylen::text::FontFamily::Selection& selection);
    static int pushLayout(lua_State* L, const haylen::text::Layout& laid, const haylen::text::Style& style, const std::function<std::shared_ptr<haylen::text::Font>(const haylen::text::Font*)>& fontOf);
    static int pushGlyph(lua_State* L, haylen::text::Font& font, std::uint32_t index);
    [[nodiscard]] static std::vector<haylen::text::Font::ShapedGlyph> shapeText(haylen::text::Font& font, std::u32string_view text, haylen::text::Direction direction, std::string_view language);

    static int measure(lua_State* L);
    static int layout(lua_State* L);
    [[nodiscard]] static float lineHeight(haylen::text::Font& font, float size);
    [[nodiscard]] static float ascent(haylen::text::Font& font, float size);
    [[nodiscard]] static float capHeight(haylen::text::Font& font, float size);
    [[nodiscard]] static float toDistance(haylen::text::Font& font, float pixels, float size);
    static int hasGlyph(lua_State* L);
    static int hasColorGlyph(lua_State* L);
    static int glyph(lua_State* L);
    static int glyphByIndex(lua_State* L);
    static int shape(lua_State* L);
    static int page(lua_State* L);
    static int nativeSize(lua_State* L);
    static int distanceField(lua_State* L);
    static int pageCount(lua_State* L);

    static int familyFace(lua_State* L);
    static int familyFallbacks(lua_State* L);
    static int familySystemFonts(lua_State* L);
    [[nodiscard]] static const std::shared_ptr<haylen::text::SystemFonts>& getSystemFonts(lua_State* L);
    static int familySelect(lua_State* L);
    static int familyResolve(lua_State* L);
    static int familyMeasure(lua_State* L);
    static int familyLayout(lua_State* L);
};

} // namespace haylen::lua::graphics
