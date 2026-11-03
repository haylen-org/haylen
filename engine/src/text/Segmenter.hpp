#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace haylen::text {

// Splits text into the pieces layout works with, by the rules of Unicode: the scripts of its runs, its grapheme clusters and the places where a line may break.
class Segmenter final {
  public:
    // What a line may do after a code point. A line takes an overflow break only when the word around it is wider than a whole line, such as after a slash of a path, so a name stays whole while it fits.
    enum class Break : std::uint8_t {
        Never,
        Overflow,
        Allowed,
        Mandatory,
    };

    // Returns the ISO 15924 tag of the script of every code point, where punctuation, digits and marks take the script of the run around them.
    [[nodiscard]] static std::vector<std::uint32_t> getScripts(std::u32string_view text);

    // Returns whether a grapheme cluster, a letter with its marks or an emoji sequence, starts at every code point.
    [[nodiscard]] static std::vector<bool> getGraphemeStarts(std::u32string_view text);

    // Returns whether a line may break after every code point by the Unicode line breaking rules, tailored to the language when it has rules of its own, such as Japanese and Chinese, and between the phrases of Thai. A slash, and a hyphen-minus inside a name, give only overflow breaks, so paths, addresses, identifiers and dates break at their joints only when they are wider than a line.
    [[nodiscard]] static std::vector<Break> getLineBreaks(std::u32string_view text, std::string_view language);

    // Tailors the breaks of a run of code from `begin` to `end`, which breaks only between its tokens: a string, from a quote to the same quote, gives only overflow breaks, and the indentation of a line never ends it.
    static void keepCode(std::u32string_view text, std::size_t begin, std::size_t end, std::vector<Break>& found);

    [[nodiscard]] static bool isSpace(char32_t codePoint) noexcept;

    // Tells whether a code point ends a paragraph by the Unicode Bidirectional Algorithm: a line feed, a carriage return, next line, the paragraph separator or an information separator from U+001C to U+001E.
    [[nodiscard]] static bool isParagraphSeparator(char32_t codePoint) noexcept;

    // Tells whether a code point draws nothing of its own, such as joiners, variation selectors, direction marks and control characters like the tab, so a font needs no glyph for it.
    [[nodiscard]] static bool isInvisible(char32_t codePoint) noexcept;

    // Tells whether a code point belongs to no script of its own, such as spaces, punctuation and digits, so it may draw with the font of the text around it.
    [[nodiscard]] static bool isCommon(char32_t codePoint) noexcept;

    // Tells whether a code point shows as an emoji unless a variation selector asks for its text form: a wide pictograph such as an emoji face or a star, a skin tone modifier or a regional indicator of a flag. Narrow pictographs such as the sun and the heart show as text unless U+FE0F asks for the emoji.
    [[nodiscard]] static bool hasEmojiPresentation(char32_t codePoint) noexcept;

    // Returns the mirrored form of a code point a right-to-left run draws, such as `)` for `(`, or the code point itself.
    [[nodiscard]] static char32_t getMirror(char32_t codePoint) noexcept;

  private:
    static constexpr char32_t kFirstSkinTone = U'\U0001F3FB';
    static constexpr char32_t kLastSkinTone = U'\U0001F3FF';
    static constexpr char32_t kFirstRegionalIndicator = U'\U0001F1E6';
    static constexpr char32_t kLastRegionalIndicator = U'\U0001F1FF';

    static void keepNames(std::u32string_view text, std::vector<Break>& found);
    static void addThaiPhrases(std::u32string_view text, std::vector<Break>& found);
    [[nodiscard]] static bool isLetterOrDigit(char32_t codePoint) noexcept;
    [[nodiscard]] static bool isQuote(char32_t codePoint) noexcept;
};

} // namespace haylen::text
