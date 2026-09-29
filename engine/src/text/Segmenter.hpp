#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace haylen::text {

// Splits text into the pieces layout works with, by the rules of Unicode: the scripts of its runs, its grapheme clusters and the places where a line may break.
class Segmenter final {
  public:
    // What a line may do after a code point.
    enum class Break : std::uint8_t {
        Never,
        Allowed,
        Mandatory,
    };

    // Returns the ISO 15924 tag of the script of every code point, where punctuation, digits and marks take the script of the run around them.
    [[nodiscard]] static std::vector<std::uint32_t> getScripts(std::u32string_view text);

    // Returns whether a grapheme cluster, a letter with its marks or an emoji sequence, starts at every code point.
    [[nodiscard]] static std::vector<bool> getGraphemeStarts(std::u32string_view text);

    // Returns whether a line may break after every code point by the Unicode line breaking rules, tailored to the language when it has rules of its own, such as Japanese and Chinese, and between the phrases of Thai.
    [[nodiscard]] static std::vector<Break> getLineBreaks(std::u32string_view text, std::string_view language);

    [[nodiscard]] static bool isSpace(char32_t codePoint) noexcept;

    // Tells whether a code point ends a paragraph by the Unicode Bidirectional Algorithm: a line feed, a carriage return, next line, the paragraph separator or an information separator from U+001C to U+001E.
    [[nodiscard]] static bool isParagraphSeparator(char32_t codePoint) noexcept;

    // Tells whether a code point draws nothing of its own, such as joiners, variation selectors, direction marks and control characters like the tab, so a font needs no glyph for it.
    [[nodiscard]] static bool isInvisible(char32_t codePoint) noexcept;

    // Tells whether a code point belongs to no script of its own, such as spaces, punctuation and digits, so it may draw with the font of the text around it.
    [[nodiscard]] static bool isCommon(char32_t codePoint) noexcept;

    // Returns the mirrored form of a code point a right-to-left run draws, such as ) for (, or the code point itself.
    [[nodiscard]] static char32_t getMirror(char32_t codePoint) noexcept;

  private:
    static void addThaiPhrases(std::u32string_view text, std::vector<Break>& found);
};

} // namespace haylen::text
