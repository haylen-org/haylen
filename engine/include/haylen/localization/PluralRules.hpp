#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace haylen::localization {

// Picks the plural category of a count by the rules of a language, the categories of the Unicode plural rules: `zero`, `one`, `two`, `few`, `many` and `other`. Cardinal rules count things, as in "3 apples", and ordinal rules rank them, as in "3rd". A language without rules of its own, or without ordinal rules, uses `other` for every count.
class PluralRules final {
  public:
    enum class Category : std::uint8_t {
        Zero,
        One,
        Two,
        Few,
        Many,
        Other,
    };

    [[nodiscard]] static Category select(std::string_view language, double count) noexcept;
    [[nodiscard]] static Category selectOrdinal(std::string_view language, double count) noexcept;

    [[nodiscard]] static std::optional<Category> categoryFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view categoryName(Category value) noexcept;

  private:
    // The operands of the Unicode rules: the absolute value, its integer digits, the count of its visible fraction digits and those digits with and without trailing zeros, as the shortest decimal form of the count shows them.
    struct Operands {
        double n = 0.0;
        std::uint64_t i = 0;
        int v = 0;
        std::uint64_t f = 0;
        std::uint64_t t = 0;
        bool integral = true;

        [[nodiscard]] bool isIntegerIn(std::uint64_t modulo, std::uint64_t low, std::uint64_t high) const noexcept;
        [[nodiscard]] bool equals(double value) const noexcept {
            return n == value;
        }
    };

    static const std::array<std::pair<std::string_view, Category>, 6> kCategoryNames;

    [[nodiscard]] static Operands operandsOf(double count) noexcept;
    [[nodiscard]] static std::string_view baseOf(std::string_view language) noexcept;
    [[nodiscard]] static bool inRange(std::uint64_t value, std::uint64_t low, std::uint64_t high) noexcept {
        return value >= low && value <= high;
    }
};

} // namespace haylen::localization
