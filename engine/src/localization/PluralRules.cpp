#include "haylen/localization/PluralRules.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace haylen::localization {

const std::array<std::pair<std::string_view, PluralRules::Category>, 6> PluralRules::kCategoryNames{{{"zero", Category::Zero}, {"one", Category::One}, {"two", Category::Two}, {"few", Category::Few}, {"many", Category::Many}, {"other", Category::Other}}};

std::optional<PluralRules::Category> PluralRules::categoryFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kCategoryNames, name, &std::pair<std::string_view, Category>::first);
    return found != kCategoryNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view PluralRules::categoryName(Category value) noexcept {
    return std::ranges::find(kCategoryNames, value, &std::pair<std::string_view, Category>::second)->first;
}

// A rule such as `n % 10 = 2..4` holds only for whole numbers.
bool PluralRules::Operands::isIntegerIn(std::uint64_t modulo, std::uint64_t low, std::uint64_t high) const noexcept {
    return integral && inRange(modulo == 0 ? i : i % modulo, low, high);
}

PluralRules::Operands PluralRules::operandsOf(double count) noexcept {
    Operands operands;
    if (!std::isfinite(count)) {
        return operands;
    }
    operands.n = std::fabs(count);
    operands.i = static_cast<std::uint64_t>(std::min(operands.n, 1.0e18));

    // The fraction digits come from the shortest form of the count with up to six decimals, as text would show it.
    std::array<char, 64> digits{};
    std::snprintf(digits.data(), digits.size(), "%.6f", operands.n);
    const std::string_view text(digits.data());
    std::string fraction(text.substr(text.find('.') + 1));
    while (!fraction.empty() && fraction.back() == '0') {
        fraction.pop_back();
    }
    operands.v = static_cast<int>(fraction.size());
    operands.f = fraction.empty() ? 0 : std::stoull(fraction);
    operands.t = operands.f;
    operands.integral = operands.v == 0;
    return operands;
}

std::string_view PluralRules::baseOf(std::string_view language) noexcept {
    return language.substr(0, language.find_first_of("-_"));
}

PluralRules::Category PluralRules::select(std::string_view language, double count) noexcept {
    const Operands o = operandsOf(count);
    const std::string_view base = baseOf(language);
    const std::uint64_t i10 = o.i % 10;
    const std::uint64_t i100 = o.i % 100;
    const bool millions = o.v == 0 && o.i != 0 && o.i % 1000000 == 0;

    if (base == "en" || base == "de" || base == "nl" || base == "sv" || base == "nb" || base == "no" || base == "nn" || base == "fi" || base == "et") {
        return o.i == 1 && o.v == 0 ? Category::One : Category::Other;
    }
    if (base == "it" || base == "ca") {
        return o.i == 1 && o.v == 0 ? Category::One : millions ? Category::Many : Category::Other;
    }
    if (base == "es") {
        return o.equals(1.0) ? Category::One : millions ? Category::Many : Category::Other;
    }
    if (base == "pt") {
        const std::string_view region = language.size() > 3 ? language.substr(3) : std::string_view();
        const bool one = region == "PT" || region == "pt" ? o.i == 1 && o.v == 0 : inRange(o.i, 0, 1);
        return one ? Category::One : millions ? Category::Many : Category::Other;
    }
    if (base == "fr") {
        return inRange(o.i, 0, 1) ? Category::One : millions ? Category::Many : Category::Other;
    }
    if (base == "da") {
        return o.equals(1.0) || (o.t != 0 && inRange(o.i, 0, 1)) ? Category::One : Category::Other;
    }
    if (base == "el" || base == "hu" || base == "tr" || base == "bg") {
        return o.equals(1.0) ? Category::One : Category::Other;
    }
    if (base == "hi" || base == "bn") {
        return o.i == 0 || o.equals(1.0) ? Category::One : Category::Other;
    }
    if (base == "ru" || base == "uk" || base == "be") {
        if (o.v != 0) {
            return Category::Other;
        }
        if (i10 == 1 && i100 != 11) {
            return Category::One;
        }
        if (inRange(i10, 2, 4) && !inRange(i100, 12, 14)) {
            return Category::Few;
        }
        return Category::Many;
    }
    if (base == "pl") {
        if (o.i == 1 && o.v == 0) {
            return Category::One;
        }
        if (o.v != 0) {
            return Category::Other;
        }
        return inRange(i10, 2, 4) && !inRange(i100, 12, 14) ? Category::Few : Category::Many;
    }
    if (base == "cs" || base == "sk") {
        if (o.v != 0) {
            return Category::Many;
        }
        return o.i == 1 ? Category::One : inRange(o.i, 2, 4) ? Category::Few : Category::Other;
    }
    if (base == "hr" || base == "sr" || base == "bs") {
        const std::uint64_t f10 = o.f % 10;
        const std::uint64_t f100 = o.f % 100;
        if ((o.v == 0 && i10 == 1 && i100 != 11) || (f10 == 1 && f100 != 11)) {
            return Category::One;
        }
        if ((o.v == 0 && inRange(i10, 2, 4) && !inRange(i100, 12, 14)) || (inRange(f10, 2, 4) && !inRange(f100, 12, 14))) {
            return Category::Few;
        }
        return Category::Other;
    }
    if (base == "sl") {
        if (o.v != 0) {
            return Category::Few;
        }
        return i100 == 1 ? Category::One : i100 == 2 ? Category::Two : inRange(i100, 3, 4) ? Category::Few : Category::Other;
    }
    if (base == "ro") {
        if (o.i == 1 && o.v == 0) {
            return Category::One;
        }
        return o.v != 0 || o.equals(0.0) || o.isIntegerIn(100, 1, 19) ? Category::Few : Category::Other;
    }
    if (base == "lt") {
        if (o.f != 0) {
            return Category::Many;
        }
        if (o.isIntegerIn(10, 1, 1) && !o.isIntegerIn(100, 11, 19)) {
            return Category::One;
        }
        return o.isIntegerIn(10, 2, 9) && !o.isIntegerIn(100, 11, 19) ? Category::Few : Category::Other;
    }
    if (base == "lv") {
        const std::uint64_t f10 = o.f % 10;
        const std::uint64_t f100 = o.f % 100;
        if (o.isIntegerIn(10, 0, 0) || o.isIntegerIn(100, 11, 19) || (o.v == 2 && inRange(f100, 11, 19))) {
            return Category::Zero;
        }
        if ((o.isIntegerIn(10, 1, 1) && !o.isIntegerIn(100, 11, 11)) || (o.v == 2 && f10 == 1 && f100 != 11) || (o.v != 2 && f10 == 1)) {
            return Category::One;
        }
        return Category::Other;
    }
    if (base == "ar") {
        if (o.equals(0.0)) {
            return Category::Zero;
        }
        if (o.equals(1.0)) {
            return Category::One;
        }
        if (o.equals(2.0)) {
            return Category::Two;
        }
        return o.isIntegerIn(100, 3, 10) ? Category::Few : o.isIntegerIn(100, 11, 99) ? Category::Many : Category::Other;
    }
    if (base == "he") {
        if ((o.i == 1 && o.v == 0) || (o.i == 0 && o.v != 0)) {
            return Category::One;
        }
        return o.i == 2 && o.v == 0 ? Category::Two : Category::Other;
    }
    if (base == "cy") {
        for (const auto& [value, category] : {std::pair{0.0, Category::Zero}, std::pair{1.0, Category::One}, std::pair{2.0, Category::Two}, std::pair{3.0, Category::Few}, std::pair{6.0, Category::Many}}) {
            if (o.equals(value)) {
                return category;
            }
        }
        return Category::Other;
    }
    if (base == "ga") {
        if (o.equals(1.0)) {
            return Category::One;
        }
        if (o.equals(2.0)) {
            return Category::Two;
        }
        return o.isIntegerIn(0, 3, 6) ? Category::Few : o.isIntegerIn(0, 7, 10) ? Category::Many : Category::Other;
    }
    return Category::Other;
}

PluralRules::Category PluralRules::selectOrdinal(std::string_view language, double count) noexcept {
    const Operands o = operandsOf(count);
    const std::string_view base = baseOf(language);
    if (!o.integral) {
        return Category::Other;
    }
    const std::uint64_t n10 = o.i % 10;
    const std::uint64_t n100 = o.i % 100;
    if (base == "en") {
        if (n10 == 1 && n100 != 11) {
            return Category::One;
        }
        if (n10 == 2 && n100 != 12) {
            return Category::Two;
        }
        return n10 == 3 && n100 != 13 ? Category::Few : Category::Other;
    }
    if (base == "fr" || base == "ro" || base == "vi" || base == "ms") {
        return o.i == 1 ? Category::One : Category::Other;
    }
    if (base == "it") {
        return o.i == 11 || o.i == 8 || o.i == 80 || o.i == 800 ? Category::Many : Category::Other;
    }
    if (base == "sv") {
        return inRange(n10, 1, 2) && !inRange(n100, 11, 12) ? Category::One : Category::Other;
    }
    if (base == "ca") {
        if (o.i == 1 || o.i == 3) {
            return Category::One;
        }
        return o.i == 2 ? Category::Two : o.i == 4 ? Category::Few : Category::Other;
    }
    if (base == "hu") {
        return o.i == 1 || o.i == 5 ? Category::One : Category::Other;
    }
    return Category::Other;
}

} // namespace haylen::localization
