#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace haylen::localization {

// Writes a moment in Unix seconds as a date, a time or both, in the patterns, month and day names and clock of a language, in the time zone of the device or at a fixed offset from UTC. Languages without names of months in the table write every style of date with numbers.
class DateFormat final {
  public:
    enum class Style : std::uint8_t {
        Short,
        Medium,
        Long,
        Full,
    };

    // A date of no style leaves the date out, so with the time it writes the time alone.
    struct Options {
        std::optional<Style> date = Style::Medium;
        bool time = false;
        std::optional<int> timeZoneOffset;
    };

    // The fields of a moment in a time zone, with the month from 1 and the day of the week from 0 for Sunday.
    struct Fields {
        std::int64_t year = 1970;
        int month = 1;
        int day = 1;
        int hour = 0;
        int minute = 0;
        int weekday = 4;
    };

    // Throws `std::invalid_argument` for an offset beyond 18 hours.
    [[nodiscard]] static std::string format(std::string_view language, std::int64_t seconds, const Options& options);

    // The fields of the moment at the offset in minutes from UTC, or in the time zone of the device without one.
    [[nodiscard]] static Fields toFields(std::int64_t seconds, std::optional<int> timeZoneOffset);

    [[nodiscard]] static std::optional<Style> styleFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view styleName(Style value) noexcept;

  private:
    static constexpr int kMaxOffsetMinutes = 18 * 60;
    static const std::array<std::pair<std::string_view, Style>, 4> kStyleNames;

    [[nodiscard]] static Fields fromUtc(std::int64_t seconds) noexcept;
    [[nodiscard]] static std::string expand(std::string_view language, std::string_view pattern, const Fields& fields);
};

} // namespace haylen::localization
