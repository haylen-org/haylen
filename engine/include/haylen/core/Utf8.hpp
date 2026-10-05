#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace haylen::core {

// Decodes and encodes UTF-8 text.
class Utf8 final {
  public:
    static constexpr char32_t kReplacementCharacter = U'\U0000FFFD';

    // Decodes the code point at `offset` and advances `offset` past it. Invalid sequences decode as U+FFFD.
    [[nodiscard]] static char32_t decode(std::string_view text, std::size_t& offset) noexcept;
    [[nodiscard]] static std::u32string decode(std::string_view text);

    // Tells whether text is well-formed UTF-8, without stray or missing continuation bytes, overlong forms, surrogates or code points above U+10FFFF.
    [[nodiscard]] static bool isValid(std::string_view text) noexcept;
    static void append(std::string& output, char32_t codePoint);
    [[nodiscard]] static std::size_t countCodePoints(std::string_view text) noexcept;

    // Returns the byte offset where the code point at the index starts, or the size of the text for an index past its end.
    [[nodiscard]] static std::size_t getOffset(std::string_view text, std::size_t index) noexcept;

  private:
    [[nodiscard]] static bool isContinuation(unsigned char byte) noexcept;
};

} // namespace haylen::core
