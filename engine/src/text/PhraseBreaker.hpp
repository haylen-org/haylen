#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace haylen::text {

// Finds where Thai text, which writes no spaces between words, may break into lines. The Thai model of BudouX weighs the letters around every place of the text and marks where a new phrase starts.
class PhraseBreaker final {
  public:
    // Returns whether a new phrase starts at every code point of a run of Thai text.
    [[nodiscard]] static std::vector<bool> findThaiPhrases(std::u32string_view text);

    [[nodiscard]] static bool isThai(char32_t codePoint) noexcept;

  private:
    // The weights of the letters and pairs and triples of letters before and after a place, in the order UW1 to UW6, BW1 to BW3 and TW1 to TW4, and the sum of every weight.
    struct Model {
        std::array<std::unordered_map<std::u32string, int>, 13> weights;
        long long total = 0;
    };

    static constexpr std::array<std::string_view, 13> kFeatures{"UW1", "UW2", "UW3", "UW4", "UW5", "UW6", "BW1", "BW2", "BW3", "TW1", "TW2", "TW3", "TW4"};

    [[nodiscard]] static const Model& getThaiModel();
    [[nodiscard]] static Model parse(std::span<const std::uint8_t> json);
    [[nodiscard]] static int weigh(const Model& model, std::size_t feature, std::u32string_view text, std::size_t begin, std::size_t length);
};

} // namespace haylen::text
