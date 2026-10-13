#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/localization/DateFormat.hpp"
#include "haylen/localization/ListFormat.hpp"
#include "haylen/localization/NumberFormat.hpp"
#include "haylen/localization/PluralRules.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::localization {

class PluralRulesTest : public ::testing::Test {
  protected:
    // Returns the categories of the counts as their names, joined by spaces.
    [[nodiscard]] static std::string categories(std::string_view language, const std::vector<double>& counts, bool ordinal = false) {
        std::string names;
        for (const double count : counts) {
            const PluralRules::Category category = ordinal ? PluralRules::selectOrdinal(language, count) : PluralRules::select(language, count);
            names += (names.empty() ? "" : " ") + std::string(PluralRules::categoryName(category));
        }
        return names;
    }
};

TEST_F(PluralRulesTest, PicksTheCategoriesOfEachLanguage) {
    EXPECT_EQ(categories("en", {0, 1, 2, 1.5}), "other one other other");
    EXPECT_EQ(categories("ru", {1, 2, 5, 11, 21, 22, 1.5}), "one few many many one few other");
    EXPECT_EQ(categories("pl", {0, 1, 2, 5, 12, 22, 1.5}), "many one few many many few other");
    EXPECT_EQ(categories("ar", {0, 1, 2, 3, 11, 100, 103}), "zero one two few many other few");
    EXPECT_EQ(categories("cy", {0, 1, 2, 3, 4, 6}), "zero one two few other many");
    EXPECT_EQ(categories("fr", {0, 1, 1.5, 2, 1000000}), "one one one other many");
    EXPECT_EQ(categories("cs", {1, 3, 5, 1.5}), "one few other many");
    EXPECT_EQ(categories("pt-BR", {0, 1, 2}), "one one other");
    EXPECT_EQ(categories("pt-PT", {0, 1, 2}), "other one other");
    EXPECT_EQ(categories("he", {1, 2, 3}), "one two other");
    EXPECT_EQ(categories("ja", {0, 1, 2}), "other other other");
    EXPECT_EQ(categories("xx", {1}), "other");
}

TEST_F(PluralRulesTest, PicksOrdinalCategories) {
    EXPECT_EQ(categories("en", {1, 2, 3, 4, 11, 12, 13, 21, 22, 23, 101}, true), "one two few other other other other one two few one");
    EXPECT_EQ(categories("fr", {1, 2}, true), "one other");
    EXPECT_EQ(categories("it", {8, 11, 9}, true), "many many other");
    EXPECT_EQ(categories("de", {1}, true), "other");
    EXPECT_EQ(PluralRules::categoryFromName("few"), PluralRules::Category::Few);
    EXPECT_FALSE(PluralRules::categoryFromName("several").has_value());
}

TEST(NumberFormatTest, WritesNumbersWithTheMarksOfALanguage) {
    const auto format = [](std::string_view language, double value, NumberFormat::Options options) { return NumberFormat::format(language, value, options); };
    EXPECT_EQ(format("en", 1234567.5, {}), "1,234,567.5");
    EXPECT_EQ(format("de", 1234567.5, {}), "1.234.567,5");
    EXPECT_EQ(format("fr", 1234567.5, {}), "1 234 567,5");
    EXPECT_EQ(format("hi", 12345678, {}), "1,23,45,678");
    EXPECT_EQ(format("en", -1234.5, {}), "-1,234.5");
    EXPECT_EQ(format("en", -0.0001, {}), "0");
    EXPECT_EQ(format("en", 3.14159, {.maximumFractionDigits = 2}), "3.14");
    EXPECT_EQ(format("en", 3, {.minimumFractionDigits = 2}), "3.00");
    EXPECT_EQ(format("en", 1234, {.grouping = false}), "1234");

    EXPECT_EQ(format("en", 0.25, {.style = NumberFormat::Style::Percent}), "25%");
    EXPECT_EQ(format("de", 0.25, {.style = NumberFormat::Style::Percent}), "25 %");
    EXPECT_EQ(format("tr", 0.25, {.style = NumberFormat::Style::Percent}), "%25");
    EXPECT_EQ(format("en", 1234.5, {.style = NumberFormat::Style::Currency}), "$1,234.50");
    EXPECT_EQ(format("de", 1234.5, {.style = NumberFormat::Style::Currency, .currency = "EUR"}), "1.234,50 €");
    EXPECT_EQ(format("en", 1235.4, {.style = NumberFormat::Style::Currency, .currency = "JPY"}), "¥1,235");
    EXPECT_EQ(format("en", 10, {.style = NumberFormat::Style::Currency, .currency = "XYZ"}), "XYZ10.00");

    EXPECT_EQ(format("en", 1234, {.style = NumberFormat::Style::Compact}), "1.2K");
    EXPECT_EQ(format("en", 12345, {.style = NumberFormat::Style::Compact}), "12K");
    EXPECT_EQ(format("en", 999950, {.style = NumberFormat::Style::Compact}), "1M");
    EXPECT_EQ(format("en", 950, {.style = NumberFormat::Style::Compact}), "950");
    EXPECT_EQ(format("ja", 12345, {.style = NumberFormat::Style::Compact}), "1.2万");
    EXPECT_EQ(format("de", 2500000, {.style = NumberFormat::Style::Compact}), "2,5 Mio.");

    EXPECT_THROW((void)format("en", 1.0 / 0.0, {}), std::invalid_argument);
    EXPECT_THROW((void)format("en", 1, {.minimumFractionDigits = 3, .maximumFractionDigits = 1}), std::invalid_argument);
}

TEST(DateFormatTest, WritesDatesAndTimesInTheStylesOfALanguage) {
    constexpr std::int64_t kMoment = 1791900300;
    // clang-format off
    const auto format = [](std::string_view language, DateFormat::Options options) {
        options.timeZoneOffset = 0;
        return DateFormat::format(language, kMoment, options);
    };
    // clang-format on
    EXPECT_EQ(format("en", {.date = DateFormat::Style::Short}), "10/13/2026");
    EXPECT_EQ(format("en", {}), "Oct 13, 2026");
    EXPECT_EQ(format("en", {.date = DateFormat::Style::Full, .time = true}), "Tuesday, October 13, 2026 2:05 PM");
    EXPECT_EQ(format("en-GB", {.date = DateFormat::Style::Long, .time = true}), "13 October 2026 14:05");
    EXPECT_EQ(format("de", {.date = DateFormat::Style::Full}), "Dienstag, 13. Oktober 2026");
    EXPECT_EQ(format("pt-BR", {.date = DateFormat::Style::Long}), "13 de outubro de 2026");
    EXPECT_EQ(format("ru", {.date = DateFormat::Style::Long}), "13 октября 2026 г.");
    EXPECT_EQ(format("ja", {.date = DateFormat::Style::Long}), "2026年10月13日");
    EXPECT_EQ(format("ko", {.date = std::nullopt, .time = true}), "오후 2:05");
    EXPECT_EQ(format("ar", {.date = DateFormat::Style::Long}), "13/10/2026");

    const DateFormat::Fields before = DateFormat::toFields(-3600, 0);
    EXPECT_EQ(std::vector<std::int64_t>({before.year, before.month, before.day, before.hour, before.weekday}), (std::vector<std::int64_t>{1969, 12, 31, 23, 3}));
    EXPECT_EQ(DateFormat::toFields(kMoment, -300).hour, 9);
    EXPECT_THROW((void)DateFormat::toFields(0, 2000), std::invalid_argument);
}

TEST(ListFormatTest, JoinsItemsInALanguage) {
    const std::vector<std::string> three{"apples", "pears", "plums"};
    EXPECT_EQ(ListFormat::format("en", three, ListFormat::Type::And), "apples, pears, and plums");
    EXPECT_EQ(ListFormat::format("en-GB", three, ListFormat::Type::And), "apples, pears and plums");
    EXPECT_EQ(ListFormat::format("en", std::vector<std::string>{"red", "blue"}, ListFormat::Type::Or), "red or blue");
    EXPECT_EQ(ListFormat::format("es", three, ListFormat::Type::And), "apples, pears y plums");
    EXPECT_EQ(ListFormat::format("ja", three, ListFormat::Type::And), "apples、pears、plums");
    EXPECT_EQ(ListFormat::format("en", std::vector<std::string>{"one"}, ListFormat::Type::And), "one");
}

TEST(LocalizationFormatLuaTest, FormatsAndPicksPluralFormsFromLua) {
    test::EngineFixture fixture({{"content/i18n/ru/voice/intro.ogg", "x"}, {"content/voice/intro.ogg", "x"}});
    // clang-format off
    fixture.runLua(R"(
        localization = require('haylen.localization')
        localization.add('en', {place = {one = '{count}st', two = '{count}nd', few = '{count}rd', other = '{count}th'}})
        localization.add('ru', {apples = {one = '{count} яблоко', few = '{count} яблока', many = '{count} яблок', other = '{count} яблока'}})
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return localization.text('place', {count = 22, ordinal = true}) .. ' ' .. localization.text('place', {count = 13, ordinal = true})"), "22nd 13th");
    EXPECT_EQ(fixture.lua("localization.setLanguage('ru') return localization.text('apples', {count = 3}) .. ', ' .. localization.text('apples', {count = 5}) .. ', ' .. localization.text('apples', {count = 21})"), "3 яблока, 5 яблок, 21 яблоко");
    EXPECT_EQ(fixture.lua("return localization.pluralCategory(5) .. ' ' .. localization.pluralCategory(3, {ordinal = true, language = 'en'})"), "many few");
    EXPECT_EQ(fixture.lua("return localization.formatNumber(1234.5, {language = 'en'}) .. ' ' .. localization.formatNumber(0.25, {style = 'percent', language = 'en'}) .. ' ' .. localization.formatNumber(1500, {style = 'compact', language = 'en'})"), "1,234.5 25% 1.5K");
    EXPECT_EQ(fixture.lua("return localization.formatNumber(9.99, {style = 'currency', currency = 'EUR', language = 'de'})"), "9,99 €");
    EXPECT_EQ(fixture.lua("return localization.formatDate(1791900300, {date = 'long', time = true, timeZoneOffset = 0, language = 'en'})"), "October 13, 2026 2:05 PM");
    EXPECT_EQ(fixture.lua("return localization.formatDate(1791900300, {date = false, time = true, timeZoneOffset = 60, language = 'de'})"), "15:05");
    EXPECT_EQ(fixture.lua("return localization.formatList({'a', 'b', 'c'}, {type = 'or', language = 'en'})"), "a, b, or c");
    EXPECT_EQ(fixture.lua("return localization.assetPath('voice/intro.ogg') .. ' ' .. localization.assetPath('voice/outro.ogg')"), "i18n/ru/voice/intro.ogg voice/outro.ogg");

    EXPECT_NE(fixture.lua("localization.formatNumber(1, {style = 'money'})").find("The style of a number must be \"decimal\", \"percent\", \"currency\" or \"compact\"."), std::string::npos);
    EXPECT_NE(fixture.lua("localization.formatNumber(1, {digits = 2})").find("Unknown option \"digits\"."), std::string::npos);
    EXPECT_NE(fixture.lua("localization.formatNumber(0 / 0)").find("A number to format must be finite."), std::string::npos);
    EXPECT_NE(fixture.lua("localization.formatDate(0, {date = 'tiny'})").find("The date of a date format must be \"short\", \"medium\", \"long\", \"full\" or false."), std::string::npos);
    EXPECT_NE(fixture.lua("localization.formatDate(0, {timeZoneOffset = 5000})").find("A time zone offset runs from -1080 to 1080 minutes."), std::string::npos);
    EXPECT_NE(fixture.lua("localization.formatList({'a'}, {type = 'xor'})").find("The type of a list must be \"and\" or \"or\"."), std::string::npos);
}

} // namespace haylen::localization
