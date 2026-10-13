#include "haylen/localization/DateFormat.hpp"

#include <algorithm>
#include <ctime>
#include <format>
#include <stdexcept>

#include "localization/LocaleData.hpp"

namespace haylen::localization {

const std::array<std::pair<std::string_view, DateFormat::Style>, 4> DateFormat::kStyleNames{{{"short", Style::Short}, {"medium", Style::Medium}, {"long", Style::Long}, {"full", Style::Full}}};

std::optional<DateFormat::Style> DateFormat::styleFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kStyleNames, name, &std::pair<std::string_view, Style>::first);
    return found != kStyleNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view DateFormat::styleName(Style value) noexcept {
    return std::ranges::find(kStyleNames, value, &std::pair<std::string_view, Style>::second)->first;
}

// The civil date of a day count since 1970, by the algorithm of the proleptic Gregorian calendar that the C++ library uses for its own dates.
DateFormat::Fields DateFormat::fromUtc(std::int64_t seconds) noexcept {
    std::int64_t days = seconds / 86400;
    std::int64_t rest = seconds % 86400;
    if (rest < 0) {
        rest += 86400;
        --days;
    }
    const std::int64_t shifted = days + 719468;
    const std::int64_t era = (shifted >= 0 ? shifted : shifted - 146096) / 146097;
    const std::int64_t dayOfEra = shifted - era * 146097;
    const std::int64_t yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
    const std::int64_t dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
    const std::int64_t monthIndex = (5 * dayOfYear + 2) / 153;
    Fields fields;
    fields.day = static_cast<int>(dayOfYear - (153 * monthIndex + 2) / 5 + 1);
    fields.month = static_cast<int>(monthIndex < 10 ? monthIndex + 3 : monthIndex - 9);
    fields.year = yearOfEra + era * 400 + (fields.month <= 2 ? 1 : 0);
    fields.hour = static_cast<int>(rest / 3600);
    fields.minute = static_cast<int>(rest % 3600 / 60);
    fields.weekday = static_cast<int>(((days % 7) + 11) % 7);
    return fields;
}

DateFormat::Fields DateFormat::toFields(std::int64_t seconds, std::optional<int> timeZoneOffset) {
    if (timeZoneOffset) {
        if (*timeZoneOffset < -kMaxOffsetMinutes || *timeZoneOffset > kMaxOffsetMinutes) {
            throw std::invalid_argument("A time zone offset runs from -1080 to 1080 minutes.");
        }
        return fromUtc(seconds + static_cast<std::int64_t>(*timeZoneOffset) * 60);
    }
    const auto moment = static_cast<std::time_t>(seconds);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &moment);
#else
    localtime_r(&moment, &local);
#endif
    return {.year = local.tm_year + 1900, .month = local.tm_mon + 1, .day = local.tm_mday, .hour = local.tm_hour, .minute = local.tm_min, .weekday = local.tm_wday};
}

std::string DateFormat::expand(std::string_view language, std::string_view pattern, const Fields& fields) {
    const LocaleData::Locale& locale = LocaleData::find(language);
    std::string text;
    while (!pattern.empty()) {
        const std::size_t open = pattern.find('{');
        if (open == std::string_view::npos) {
            text.append(pattern);
            break;
        }
        text.append(pattern.substr(0, open));
        const std::size_t close = pattern.find('}', open);
        const std::string_view field = pattern.substr(open + 1, close - open - 1);
        pattern.remove_prefix(close + 1);

        const int hour12 = fields.hour % 12 == 0 ? 12 : fields.hour % 12;
        if (field == "d") {
            text += std::to_string(fields.day);
        } else if (field == "dd") {
            text += std::format("{:02}", fields.day);
        } else if (field == "M") {
            text += std::to_string(fields.month);
        } else if (field == "MM") {
            text += std::format("{:02}", fields.month);
        } else if (field == "MMM") {
            text.append(locale.shortMonths.at(static_cast<std::size_t>(fields.month - 1)));
        } else if (field == "MMMM") {
            text.append(locale.months.at(static_cast<std::size_t>(fields.month - 1)));
        } else if (field == "y") {
            text += std::to_string(fields.year);
        } else if (field == "EEEE") {
            text.append(locale.days.at(static_cast<std::size_t>(fields.weekday)));
        } else if (field == "H") {
            text += std::to_string(fields.hour);
        } else if (field == "HH") {
            text += std::format("{:02}", fields.hour);
        } else if (field == "h") {
            text += std::to_string(hour12);
        } else if (field == "mm") {
            text += std::format("{:02}", fields.minute);
        } else if (field == "a") {
            text.append(fields.hour < 12 ? locale.am : locale.pm);
        }
    }
    return text;
}

std::string DateFormat::format(std::string_view language, std::int64_t seconds, const Options& options) {
    const Fields fields = toFields(seconds, options.timeZoneOffset);
    const LocaleData::Locale& locale = LocaleData::find(language);
    std::string text;
    if (options.date) {
        text = expand(language, locale.dates.at(static_cast<std::size_t>(*options.date)), fields);
    }
    if (options.time) {
        text += (text.empty() ? "" : " ") + expand(language, locale.time, fields);
    }
    return text;
}

} // namespace haylen::localization
