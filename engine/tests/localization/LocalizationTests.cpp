#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/localization/Catalog.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::localization {

class CatalogTest : public ::testing::Test {
  protected:
    [[nodiscard]] static Catalog makeEnglish() {
        Catalog catalog;
        // clang-format off
        catalog.add("en", core::Json::parse(R"({
            "menu": {"play": "Play", "quit": "Quit"},
            "greeting": "Hello, {name}!",
            "coins": {"zero": "No coins", "one": "{count} coin", "other": "{count} coins"},
            "trees": {"other": "{count} trees"}
        })"));
        // clang-format on
        return catalog;
    }
};

TEST_F(CatalogTest, TranslatesFormatsAndPluralizes) {
    const Catalog catalog = makeEnglish();
    EXPECT_EQ(catalog.getLanguage(), "en");
    EXPECT_EQ(catalog.getFallback(), "en");
    EXPECT_EQ(catalog.getText("menu.play"), "Play");
    EXPECT_EQ(catalog.getText("greeting", {{"name", "Ana"}}), "Hello, Ana!");
    EXPECT_EQ(catalog.getText("coins", {{"count", 0}}), "No coins");
    EXPECT_EQ(catalog.getText("coins", {{"count", 1}}), "1 coin");
    EXPECT_EQ(catalog.getText("coins", {{"count", 5}}), "5 coins");
    EXPECT_EQ(catalog.getText("coins", {{"count", 2.5}}), "2.5 coins");
    EXPECT_EQ(catalog.getText("coins", {{"count", 3.0}}), "3 coins");
    EXPECT_EQ(catalog.getText("coins"), "{count} coins");
    EXPECT_EQ(catalog.getText("trees", {{"count", 1}}), "1 trees");
    EXPECT_EQ(catalog.getText("trees", {{"count", 0}}), "0 trees");
    EXPECT_EQ(catalog.getText("missing.key"), "missing.key");
    EXPECT_TRUE(catalog.has("menu.quit"));
    EXPECT_FALSE(catalog.has("menu"));
}

TEST_F(CatalogTest, FallsBackAndMergesLanguages) {
    Catalog catalog = makeEnglish();
    catalog.add("pt-BR", core::Json::parse(R"({"menu": {"play": "Jogar"}})"));
    catalog.add("en", core::Json::parse(R"({"menu": {"settings": "Settings"}})"));
    EXPECT_EQ(catalog.getLanguages(), (std::vector<std::string>{"en", "pt-BR"}));
    EXPECT_EQ(catalog.getLanguage(), "en");

    catalog.setLanguage("pt-BR");
    EXPECT_EQ(catalog.getText("menu.play"), "Jogar");
    EXPECT_EQ(catalog.getText("menu.quit"), "Quit");
    EXPECT_EQ(catalog.getText("menu.settings"), "Settings");

    catalog.setFallback("pt-BR");
    EXPECT_EQ(catalog.getText("menu.quit"), "menu.quit");
    EXPECT_THROW(catalog.setLanguage("fr"), std::invalid_argument);
    EXPECT_THROW(catalog.setFallback("fr"), std::invalid_argument);
}

TEST_F(CatalogTest, HandlesBracesAndGroupsThatLookLikePlurals) {
    Catalog catalog;
    catalog.add("en", core::Json::parse(R"({"note": "{{literal}} {missing} {flag} {n} {oops", "odd": {"few": "a", "other": "b"}})"));
    EXPECT_EQ(catalog.getText("note", {{"flag", true}, {"n", 3}}), "{literal} {missing} true 3 {oops");
    EXPECT_EQ(catalog.getText("odd.few"), "a");
    EXPECT_EQ(catalog.getText("odd.other"), "b");
    EXPECT_FALSE(catalog.has("odd"));

    EXPECT_THROW(catalog.add("", core::Json::object()), std::invalid_argument);
    EXPECT_THROW(catalog.add("en", core::Json::array()), std::invalid_argument);
    EXPECT_THROW(catalog.add("en", core::Json::parse(R"({"menu": {"count": 3}})")), std::invalid_argument);
    EXPECT_THROW((void)catalog.getText("note", core::Json::array()), std::invalid_argument);
}

TEST_F(CatalogTest, MatchesLanguageTags) {
    Catalog catalog;
    for (const char* language : {"en", "pt", "pt-PT", "es-MX"}) {
        catalog.add(language, core::Json::object());
    }
    EXPECT_EQ(catalog.findBestMatch("pt-BR"), "pt");
    EXPECT_EQ(catalog.findBestMatch("PT_pt"), "pt-PT");
    EXPECT_EQ(catalog.findBestMatch("es"), "es-MX");
    EXPECT_EQ(catalog.findBestMatch("EN"), "en");
    EXPECT_EQ(catalog.findBestMatch("de-DE"), std::nullopt);
}

// A table declares the direction of its language, which is left to right until a table says otherwise.
TEST_F(CatalogTest, DeclaresTheDirectionOfALanguage) {
    Catalog catalog = makeEnglish();
    catalog.add("ar", core::Json::parse(R"({"@direction": "rightToLeft", "menu": {"play": "العب"}})"));
    catalog.add("ar", core::Json::parse(R"({"menu": {"quit": "خروج"}})"));
    EXPECT_EQ(catalog.getDirection("en"), text::Direction::LeftToRight);
    EXPECT_EQ(catalog.getDirection("ar"), text::Direction::RightToLeft);
    catalog.setLanguage("ar");
    EXPECT_EQ(catalog.getText("menu.play"), "العب");
    EXPECT_FALSE(catalog.has("@direction"));
    EXPECT_THROW(catalog.add("he", core::Json::parse(R"({"@direction": "up"})")), std::invalid_argument);
    EXPECT_THROW((void)catalog.getDirection("xx"), std::invalid_argument);
}

TEST(LocalizationLuaTest, LoadsLanguageFoldersFromThePackage) {
    // clang-format off
    test::EngineFixture fixture({
        {"content/i18n/en.json", R"({"hud": {"day": "Day {day}", "wood": {"one": "{count} log", "other": "{count} logs"}}})"},
        {"content/i18n/pt-BR.json", R"({"hud": {"day": "Dia {day}"}})"},
        {"content/i18n/notes.txt", "ignored"},
        {"content/broken/en.json", "{"},
    });
    // clang-format on
    fixture.runLua("localization = require('haylen.localization')");

    EXPECT_EQ(fixture.lua("return table.concat(localization.loadFolder('i18n'), ',')"), "en,pt-BR");
    EXPECT_EQ(fixture.lua("return localization.language() .. ' ' .. localization.fallback() .. ' ' .. #localization.languages()"), "en en 2");
    EXPECT_EQ(fixture.lua("return localization.text('hud.day', {day = 3}) .. ' / ' .. localization.text('hud.wood', {count = 1})"), "Day 3 / 1 log");
    EXPECT_EQ(fixture.lua("localization.setLanguage(localization.findBestMatch('pt_BR')) return localization.text('hud.day', {day = 4}) .. ' / ' .. localization.text('hud.wood', {count = 7})"), "Dia 4 / 7 logs");
    EXPECT_EQ(fixture.lua("localization.add('fr', {hud = {day = 'Jour {day}'}}) localization.setFallback('fr') return localization.fallback() .. ' ' .. tostring(localization.has('hud.wood'))"), "fr false");
    EXPECT_EQ(fixture.lua("return tostring(localization.findBestMatch('de')) .. ' ' .. localization.text('nothing.here')"), "nil nothing.here");

    EXPECT_EQ(fixture.lua("localization.add('ar', {['@direction'] = 'rightToLeft', hud = {day = 'اليوم {day}'}}) return localization.direction('ar') .. ' ' .. localization.direction()"), "rightToLeft leftToRight");
    EXPECT_NE(fixture.lua("localization.loadFolder('broken')").find("broken/en.json is not valid JSON"), std::string::npos);
    EXPECT_NE(fixture.lua("localization.setLanguage('xx')").find("No localization table was added for xx"), std::string::npos);
    EXPECT_NE(fixture.lua("localization.add('de', {menu = {count = 3}})").find("menu.count"), std::string::npos);
}

} // namespace haylen::localization
