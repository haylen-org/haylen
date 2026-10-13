#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace haylen::localization {

// The marks, patterns and names that numbers, dates and lists use in a curated set of languages, from the common data of each language. A language that the table lacks takes the table of its base language, and a base language that the table lacks takes English.
class LocaleData final {
  public:
    // A short scale of a compact number, such as 3 and `K` for thousands, or 4 and `万` for the ten thousands of Japanese and Chinese. A power of 0 marks an unused entry.
    struct Compact {
        int power = 0;
        std::string_view suffix;
    };

    // The patterns hold `{n}` for the number, `{s}` for the currency, and the fields of a date: `{d}`, `{dd}`, `{M}`, `{MM}`, `{MMM}`, `{MMMM}`, `{y}`, `{EEEE}`, `{H}`, `{HH}`, `{h}`, `{mm}` and `{a}`. Months and days hold the forms that dates use, with Sunday first.
    struct Locale {
        std::string_view tag;
        std::string_view decimal;
        std::string_view group;
        std::uint8_t secondaryGroup = 3;
        std::string_view percent;
        std::string_view currency;
        std::array<Compact, 4> compact{};
        std::array<std::string_view, 4> dates{};
        std::string_view time;
        std::string_view am;
        std::string_view pm;
        std::array<std::string_view, 12> months{};
        std::array<std::string_view, 12> shortMonths{};
        std::array<std::string_view, 7> days{};
        std::string_view listSeparator;
        std::string_view andTwo;
        std::string_view andLast;
        std::string_view orTwo;
        std::string_view orLast;
    };

    [[nodiscard]] static const Locale& find(std::string_view language) noexcept;

  private:
    static const std::array<Locale, 29> kLocales;
};

} // namespace haylen::localization
