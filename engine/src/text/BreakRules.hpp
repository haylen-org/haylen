#pragma once

namespace haylen::text {

// Where lines may wrap: after spaces, which a wrapped line drops at its end, and before or after any ideograph or kana, since Chinese and Japanese text has no spaces between words.
class BreakRules final {
  public:
    [[nodiscard]] static constexpr bool isSpace(char32_t codePoint) noexcept {
        return codePoint == U' ' || codePoint == U'\t' || codePoint == U'　';
    }

    [[nodiscard]] static constexpr bool isIdeograph(char32_t codePoint) noexcept {
        return (codePoint >= U'⺀' && codePoint <= U'鿿') || (codePoint >= U'가' && codePoint <= U'힯') || (codePoint >= U'豈' && codePoint <= U'﫿') || (codePoint >= U'\U00020000' && codePoint <= U'\U0003FFFF');
    }

    // Tells whether a line may wrap between two neighbouring code points, which never leaves a space at the start of a line.
    [[nodiscard]] static constexpr bool canBreakBetween(char32_t before, char32_t after) noexcept {
        return !isSpace(after) && (isSpace(before) || isIdeograph(before) || isIdeograph(after));
    }
};

} // namespace haylen::text
