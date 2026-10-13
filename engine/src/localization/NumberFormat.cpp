#include "haylen/localization/NumberFormat.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <stdexcept>

#include "localization/LocaleData.hpp"

namespace haylen::localization {

const std::array<std::pair<std::string_view, NumberFormat::Style>, 4> NumberFormat::kStyleNames{{{"decimal", Style::Decimal}, {"percent", Style::Percent}, {"currency", Style::Currency}, {"compact", Style::Compact}}};

const std::array<std::pair<std::string_view, std::string_view>, 21> NumberFormat::kCurrencySymbols{{
    {"USD", "$"}, {"EUR", "€"}, {"GBP", "£"}, {"JPY", "¥"}, {"CNY", "¥"}, {"KRW", "₩"}, {"BRL", "R$"}, {"INR", "₹"}, {"RUB", "₽"}, {"CAD", "CA$"}, {"AUD", "A$"}, {"MXN", "MX$"}, {"CHF", "CHF"}, {"SEK", "kr"}, {"NOK", "kr"}, {"DKK", "kr."}, {"PLN", "zł"}, {"CZK", "Kč"}, {"TRY", "₺"}, {"ILS", "₪"}, {"UAH", "₴"},
}};

std::optional<NumberFormat::Style> NumberFormat::styleFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kStyleNames, name, &std::pair<std::string_view, Style>::first);
    return found != kStyleNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view NumberFormat::styleName(Style value) noexcept {
    return std::ranges::find(kStyleNames, value, &std::pair<std::string_view, Style>::second)->first;
}

std::string_view NumberFormat::getCurrencySymbol(std::string_view code) noexcept {
    const auto found = std::ranges::find(kCurrencySymbols, code, &std::pair<std::string_view, std::string_view>::first);
    return found != kCurrencySymbols.end() ? found->second : code;
}

int NumberFormat::getCurrencyDigits(std::string_view code) noexcept {
    return code == "JPY" || code == "KRW" ? 0 : 2;
}

std::string NumberFormat::replace(std::string_view pattern, std::string_view key, std::string_view value) {
    std::string result(pattern);
    const std::size_t found = result.find(key);
    if (found != std::string::npos) {
        result.replace(found, key.size(), value);
    }
    return result;
}

std::string NumberFormat::formatPlain(std::string_view language, double value, int minimum, int maximum, bool grouping) {
    const LocaleData::Locale& locale = LocaleData::find(language);
    std::string digits = std::format("{:.{}f}", std::fabs(value), maximum);
    std::string fraction;
    if (const std::size_t point = digits.find('.'); point != std::string::npos) {
        fraction = digits.substr(point + 1);
        digits.resize(point);
        while (static_cast<int>(fraction.size()) > minimum && fraction.back() == '0') {
            fraction.pop_back();
        }
    }

    // The first group from the right holds three digits and the others the secondary size, which is two in Indian grouping.
    std::string whole;
    std::size_t size = 3;
    std::size_t count = 0;
    for (auto digit = digits.rbegin(); digit != digits.rend(); ++digit) {
        if (grouping && count == size) {
            whole.insert(0, locale.group);
            count = 0;
            size = locale.secondaryGroup;
        }
        whole.insert(whole.begin(), *digit);
        ++count;
    }
    return fraction.empty() ? whole : whole + std::string(locale.decimal) + fraction;
}

std::string NumberFormat::format(std::string_view language, double value, const Options& options) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument("A number to format must be finite.");
    }
    const LocaleData::Locale& locale = LocaleData::find(language);
    double shown = options.style == Style::Percent ? value * 100.0 : value;
    std::string_view suffix;

    // A compact number takes the largest scale it reaches, and moves up a scale when rounding reaches the next one.
    int defaultMinimum = 0;
    int defaultMaximum = options.style == Style::Decimal ? 3 : 0;
    if (options.style == Style::Currency) {
        defaultMinimum = defaultMaximum = getCurrencyDigits(options.currency);
    }
    if (options.style == Style::Compact) {
        int power = 0;
        for (const LocaleData::Compact& scale : locale.compact) {
            const double units = std::fabs(value) / std::pow(10.0, power);
            const double displayed = units < 9.95 ? std::round(units * 10.0) / 10.0 : std::round(units);
            if (scale.power > 0 && displayed * std::pow(10.0, power) >= std::pow(10.0, scale.power)) {
                power = scale.power;
                shown = value / std::pow(10.0, power);
                suffix = scale.suffix;
            }
        }
        defaultMaximum = std::fabs(shown) < 9.95 ? 1 : 0;
    }
    const int minimum = options.minimumFractionDigits.value_or(std::min(defaultMinimum, options.maximumFractionDigits.value_or(defaultMinimum)));
    const int maximum = options.maximumFractionDigits.value_or(std::max(defaultMaximum, minimum));
    if (minimum < 0 || maximum > kMaxFractionDigits || minimum > maximum) {
        throw std::invalid_argument("Fraction digits run from 0 to 20, with the minimum not above the maximum.");
    }

    std::string number = formatPlain(language, shown, minimum, maximum, options.grouping) + std::string(suffix);
    std::string text;
    switch (options.style) {
    case Style::Percent:
        text = replace(locale.percent, "{n}", number);
        break;
    case Style::Currency:
        text = replace(replace(locale.currency, "{n}", number), "{s}", getCurrencySymbol(options.currency));
        break;
    case Style::Decimal:
    case Style::Compact:
        text = std::move(number);
        break;
    }
    const bool negative = std::signbit(value) && std::stod(std::format("{:.{}f}", std::fabs(shown), maximum)) != 0.0;
    return negative ? "-" + text : text;
}

} // namespace haylen::localization
