#include <gtest/gtest.h>

#include <string>

#include "support/LuaFixture.hpp"

namespace haylen::lua::localization {

TEST(LocalizationFormatLuaTest, FormatsAndPicksPluralFormsFromLua) {
    test::LuaFixture fixture({{"content/i18n/ru/voice/intro.ogg", "x"}, {"content/voice/intro.ogg", "x"}});
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

    for (const char* moment : {"0/0", "math.huge", "-math.huge", "1e300"}) {
        EXPECT_NE(fixture.lua(std::string("localization.formatDate(") + moment + ")").find("A moment to format is a finite number of seconds within the range of 64-bit integers."), std::string::npos) << moment;
    }

    EXPECT_NE(fixture.lua("localization.formatDate(math.maxinteger - 1023, {timeZoneOffset = 60})").find("The moment is too far from 1970 to shift by the time zone offset."), std::string::npos);
    EXPECT_NE(fixture.lua("localization.formatDate(9e18)").find("The system cannot express the moment 9000000000000000000 in the time zone of the device."), std::string::npos);
    EXPECT_NE(fixture.lua("localization.formatList({'a'}, {type = 'xor'})").find("The type of a list must be \"and\" or \"or\"."), std::string::npos);
}

TEST(LocalizationFormatLuaTest, ReadsTheLanguageOptionInAnyFormOfItsTag) {
    test::LuaFixture fixture;
    fixture.runLua("localization = require('haylen.localization')");
    EXPECT_EQ(fixture.lua("return localization.pluralCategory(3, {language = 'RU'}) .. ' ' .. localization.formatNumber(1234.5, {language = 'PT_br'})"), "few 1.234,5");
    EXPECT_NE(fixture.lua("localization.formatList({'a'}, {language = 'not a tag'})").find("The language tag \"not a tag\" is not valid."), std::string::npos);
}

} // namespace haylen::lua::localization
