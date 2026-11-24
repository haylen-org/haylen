#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/localization/DateFormat.hpp"
#include "haylen/localization/ListFormat.hpp"
#include "haylen/localization/NumberFormat.hpp"
#include "haylen/localization/PluralRules.hpp"

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

TEST_F(PluralRulesTest, PicksTheCategoryOfCountsWithMoreDigitsThanAnyNumberType) {
    EXPECT_EQ(categories("en", {1e62, 1.2345e70, 1e300}), "other other other");
    EXPECT_EQ(categories("en", {1e62}, true), "other");
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

    EXPECT_THROW((void)format("en", std::numeric_limits<double>::infinity(), {}), std::invalid_argument);
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

} // namespace haylen::localization
