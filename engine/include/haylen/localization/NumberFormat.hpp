#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace haylen::localization {

// Writes numbers with the decimal mark, the grouping, the percent sign, the currency place and the short scales of a language: decimals, percentages of a ratio, amounts of a currency by its ISO code and compact numbers such as `1.2K` or `1,2 Mio.`.
class NumberFormat final {
  public:
    enum class Style : std::uint8_t {
        Decimal,
        Percent,
        Currency,
        Compact,
    };

    // Fraction digits default to 0 to 3 for decimals, 0 for percentages, the digits of the currency for amounts and one decimal for compact numbers below ten.
    struct Options {
        Style style = Style::Decimal;
        std::optional<int> minimumFractionDigits;
        std::optional<int> maximumFractionDigits;
        std::string currency = "USD";
        bool grouping = true;
    };

    // Throws `std::invalid_argument` for a number that is not finite and for fraction digits outside 0 to 20 or a minimum above the maximum.
    [[nodiscard]] static std::string format(std::string_view language, double value, const Options& options);

    [[nodiscard]] static std::optional<Style> styleFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view styleName(Style value) noexcept;

  private:
    static constexpr int kMaxFractionDigits = 20;
    static const std::array<std::pair<std::string_view, Style>, 4> kStyleNames;
    static const std::array<std::pair<std::string_view, std::string_view>, 21> kCurrencySymbols;

    [[nodiscard]] static std::string_view getCurrencySymbol(std::string_view code) noexcept;
    [[nodiscard]] static int getCurrencyDigits(std::string_view code) noexcept;

    // Writes the absolute value with the fraction digits and the marks of the language.
    [[nodiscard]] static std::string formatPlain(std::string_view language, double value, int minimum, int maximum, bool grouping);
    [[nodiscard]] static std::string replace(std::string_view pattern, std::string_view key, std::string_view value);
};

} // namespace haylen::localization
