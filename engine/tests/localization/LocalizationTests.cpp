#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/core/AppConfig.hpp"
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

TEST_F(CatalogTest, TranslatesIntoAPseudoLanguageThatShowsOverflowAndGaps) {
    Catalog catalog = makeEnglish();
    catalog.add("en", core::Json::parse(R"({"story": "[b]Once[/b] upon {{a}} time"})"));
    catalog.setPseudo(true);
    EXPECT_TRUE(catalog.isPseudo());
    EXPECT_EQ(catalog.getText("menu.play"), "« Pláý .. »");
    EXPECT_EQ(catalog.getText("greeting", {{"name", "Ana"}}), "« Hélló, Ana! .. »");
    EXPECT_EQ(catalog.getText("coins", {{"count", 5}}), "« 5 çóíñs .. »");
    EXPECT_EQ(catalog.getText("story"), "« [b]Óñçé[/b] úpóñ {a} tímé .... »");
    EXPECT_EQ(catalog.getText("missing.key"), "missing.key");
    catalog.setPseudo(false);
    EXPECT_EQ(catalog.getText("menu.play"), "Play");
}

TEST_F(CatalogTest, ShowsKeysUnchangedBeforeAnyLanguageIsAdded) {
    const Catalog catalog;
    EXPECT_TRUE(catalog.getLanguage().empty());
    EXPECT_FALSE(catalog.has("menu.play"));
    EXPECT_EQ(catalog.getText("menu.play", {{"count", 2}}), "menu.play");
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

    catalog.setFallback("pt_br");
    EXPECT_EQ(catalog.getFallback(), "pt-BR");
    EXPECT_EQ(catalog.getText("menu.quit"), "menu.quit");
    EXPECT_THROW(catalog.setFallback("fr"), std::invalid_argument);
}

TEST_F(CatalogTest, HandlesBracesAndGroupsThatLookLikePlurals) {
    Catalog catalog;
    catalog.add("en", core::Json::parse(R"({"note": "{{literal}} {missing} {flag} {n} {oops", "odd": {"several": "a", "other": "b"}})"));
    EXPECT_EQ(catalog.getText("note", {{"flag", true}, {"n", 3}}), "{literal} {missing} true 3 {oops");
    EXPECT_EQ(catalog.getText("odd.several"), "a");
    EXPECT_EQ(catalog.getText("odd.other"), "b");
    EXPECT_FALSE(catalog.has("odd"));

    EXPECT_THROW(catalog.add("", core::Json::object()), std::invalid_argument);
    EXPECT_THROW(catalog.add("en", core::Json::array()), std::invalid_argument);
    EXPECT_THROW(catalog.add("en", core::Json::parse(R"({"menu": {"count": 3}})")), std::invalid_argument);
    EXPECT_THROW((void)catalog.getText("note", core::Json::array()), std::invalid_argument);
}

TEST_F(CatalogTest, FormatsTextsOutsideTheCatalogLikeItsOwn) {
    EXPECT_EQ(Catalog::format("You carry {gold} coins, {{gold}} {missing}.", {{"gold", 30}}), "You carry 30 coins, {gold} {missing}.");
    EXPECT_THROW((void)Catalog::format("{gold}", core::Json::array()), std::invalid_argument);
}

TEST_F(CatalogTest, NormalizesLanguageTags) {
    EXPECT_EQ(Catalog::normalizeTag("en"), "en");
    EXPECT_EQ(Catalog::normalizeTag("EN_us"), "en-US");
    EXPECT_EQ(Catalog::normalizeTag("pt_br"), "pt-BR");
    EXPECT_EQ(Catalog::normalizeTag("zh_hans_cn"), "zh-Hans-CN");
    EXPECT_EQ(Catalog::normalizeTag("es-419"), "es-419");
    EXPECT_EQ(Catalog::normalizeTag("ca-es-VALENCIA"), "ca-ES-valencia");
    EXPECT_EQ(Catalog::normalizeTag("de-CH-1901"), "de-CH-1901");
    EXPECT_EQ(Catalog::normalizeTag("en-US-u-mu-celsius"), "en-US");
    EXPECT_EQ(Catalog::normalizeTag("fil-PH-x-private"), "fil-PH");
    for (const char* text : {"", "e", "english", "en-", "en--US", "en US", "pt-BR-", "123", "en-Latn-Latn", "en-US-BR", "x-private", "pt.BR"}) {
        EXPECT_THROW((void)Catalog::normalizeTag(text), std::invalid_argument) << text;
    }
}

// The tag itself wins, then the tag without its last subtags, then the first language added of the same language whose script agrees.
TEST_F(CatalogTest, MatchesLanguageTagsInOrder) {
    Catalog catalog;
    for (const char* language : {"en", "pt", "pt-PT", "es-MX", "es-AR", "zh-Hans", "zh-Hant-TW"}) {
        catalog.add(language, core::Json::object());
    }
    EXPECT_EQ(catalog.findBestMatch("PT_pt"), "pt-PT");
    EXPECT_EQ(catalog.findBestMatch("pt-BR"), "pt");
    EXPECT_EQ(catalog.findBestMatch("en-Latn-US"), "en");
    EXPECT_EQ(catalog.findBestMatch("es"), "es-MX");
    EXPECT_EQ(catalog.findBestMatch("es-CL"), "es-MX");
    EXPECT_EQ(catalog.findBestMatch("zh-Hans-CN"), "zh-Hans");
    EXPECT_EQ(catalog.findBestMatch("zh-Hant-HK"), "zh-Hant-TW");
    EXPECT_EQ(catalog.findBestMatch("zh-TW"), "zh-Hans");
    EXPECT_EQ(catalog.findBestMatch("de-DE"), std::nullopt);
    EXPECT_THROW((void)catalog.findBestMatch("pt BR"), std::invalid_argument);
}

// Without a choice of the app, the first language of the system with a close language wins, then the fallback, which is the first language added until the app names another.
TEST_F(CatalogTest, FollowsTheLanguagesOfTheSystem) {
    Catalog catalog;
    catalog.add("ja", core::Json::object());
    catalog.add("en", core::Json::object());
    catalog.add("pt_BR", core::Json::object());
    EXPECT_TRUE(catalog.isFollowingSystem());
    EXPECT_EQ(catalog.getLanguage(), "ja");
    EXPECT_EQ(catalog.getLanguages(), (std::vector<std::string>{"ja", "en", "pt-BR"}));

    catalog.setSystemLanguages(std::vector<std::string>{"fr-FR", "pt-PT", "en"});
    EXPECT_EQ(catalog.getLanguage(), "pt-BR");
    catalog.setSystemLanguages(std::vector<std::string>{"de"});
    EXPECT_EQ(catalog.getLanguage(), "ja");
    catalog.setFallback("en");
    EXPECT_EQ(catalog.getLanguage(), "en");

    // A language added later that is closer to the system takes over.
    catalog.add("de-DE", core::Json::object());
    EXPECT_EQ(catalog.getLanguage(), "de-DE");
    EXPECT_THROW(catalog.setSystemLanguages(std::vector<std::string>{"not a tag"}), std::invalid_argument);
}

// The choice of the app outlives changes of the system and languages added later, and falls back like the system does.
TEST_F(CatalogTest, KeepsTheChoiceOfTheAppUntilItFollowsTheSystemAgain) {
    Catalog catalog;
    catalog.setLanguage("PT_br");
    EXPECT_FALSE(catalog.isFollowingSystem());
    EXPECT_EQ(catalog.getLanguage(), "");
    catalog.add("en", core::Json::object());
    EXPECT_EQ(catalog.getLanguage(), "en");
    catalog.add("pt-BR", core::Json::object());
    EXPECT_EQ(catalog.getLanguage(), "pt-BR");

    catalog.setSystemLanguages(std::vector<std::string>{"en-GB"});
    EXPECT_EQ(catalog.getLanguage(), "pt-BR");
    catalog.setLanguage("de");
    EXPECT_EQ(catalog.getLanguage(), "en");
    catalog.followSystem();
    EXPECT_TRUE(catalog.isFollowingSystem());
    EXPECT_EQ(catalog.getLanguage(), "en");
    EXPECT_THROW(catalog.setLanguage("pt-"), std::invalid_argument);
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
        {"content/i18n/pt_BR.json", R"({"hud": {"day": "Dia {day}"}})"},
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
    EXPECT_EQ(fixture.lua("return localization.format('Day {day} of {days}', {day = 2, days = 7}) .. ' / ' .. localization.format('No {day}')"), "Day 2 of 7 / No {day}");
    EXPECT_EQ(fixture.lua("localization.setPseudo(true) local text = localization.text('hud.day', {day = 5}) localization.setPseudo(false) return text .. ' ' .. tostring(localization.pseudo())"), "« Díá 5 . » false");

    EXPECT_EQ(fixture.lua("localization.add('ar', {['@direction'] = 'rightToLeft', hud = {day = 'اليوم {day}'}}) return localization.direction('ar') .. ' ' .. localization.direction()"), "rightToLeft leftToRight");
    EXPECT_NE(fixture.lua("localization.loadFolder('broken')").find("broken/en.json\" is not valid JSON"), std::string::npos);
    EXPECT_NE(fixture.lua("localization.setLanguage('pt BR')").find("The language tag \"pt BR\" is not valid."), std::string::npos);
    EXPECT_NE(fixture.lua("localization.setFallback('xx')").find("No localization table was added for \"xx\""), std::string::npos);
    EXPECT_EQ(fixture.lua("return localization.normalizeTag('zh_hant_tw')"), "zh-Hant-TW");
    EXPECT_NE(fixture.lua("localization.add('de', {menu = {count = 3}})").find("menu.count"), std::string::npos);
}

TEST(LocalizationLuaTest, RefusesFilesNotNamedAfterALanguage) {
    test::EngineFixture fixture({{"content/i18n/english.json", "{}"}});
    fixture.runLua("localization = require('haylen.localization')");
    EXPECT_NE(fixture.lua("localization.loadFolder('i18n')").find("The localization file \"i18n/english.json\" is not named after a language tag."), std::string::npos);
}

// The section `localization` of app.json loads the languages before any script runs, the system picks among them, and every change of the current language reaches the app once, at the start of the next frame.
TEST(LocalizationPluginTest, FollowsTheSystemAndAnnouncesEveryChangeOfLanguage) {
    // clang-format off
    test::EngineFixture fixture({
        {"app.json", R"({"localization": {"folder": "locale", "fallback": "en"}})"},
        {"content/locale/en.json", R"({"menu": {"play": "Play"}})"},
        {"content/locale/es.json", R"({"menu": {"play": "Jugar"}})"},
        {"content/locale/pt-BR.json", R"({"menu": {"play": "Jogar"}})"},
    });
    // clang-format on
    fixture.host().setLanguages(std::vector<std::string>{"pt_PT", "en"});
    fixture.restart();
    // clang-format off
    fixture.runLua(R"(
        localization = require('haylen.localization')
        events = require('haylen.events')
        heard = {}
        events.on('languageChanged', function(data) heard[#heard + 1] = data.previous .. '>' .. data.language end)
        events.on('systemLanguagesChanged', function(data) heard[#heard + 1] = 'system ' .. table.concat(data.languages, ',') end)
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return localization.language() .. ' ' .. localization.text('menu.play') .. ' ' .. tostring(localization.followingSystem())"), "pt-BR Jogar true");

    fixture.host().setLanguages(std::vector<std::string>{"es-MX"});
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat(heard, ', ')"), "system es-MX, pt-BR>es");

    fixture.runLua("localization.setLanguage('pt') localization.setLanguage('en_US')");
    fixture.host().setLanguages(std::vector<std::string>{"pt-BR"});
    fixture.frames(2);
    EXPECT_EQ(fixture.lua("return table.concat(heard, ', ') .. ' / ' .. localization.language()"), "system es-MX, pt-BR>es, system pt-BR, es>en / en");

    fixture.runLua("localization.followSystem()");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return heard[#heard] .. ' ' .. localization.text('menu.play')"), "en>pt-BR Jogar");
}

// A language that app.json names is the choice of the app from the start.
TEST(LocalizationPluginTest, StartsInTheLanguageOfAppJson) {
    // clang-format off
    test::EngineFixture fixture({
        {"app.json", R"({"localization": {"folder": "locale", "language": "es_419"}})"},
        {"content/locale/en.json", "{}"},
        {"content/locale/es.json", "{}"},
    });
    // clang-format on
    fixture.host().setLanguages(std::vector<std::string>{"en"});
    fixture.restart();
    fixture.runLua("localization = require('haylen.localization')");
    EXPECT_EQ(fixture.lua("return localization.language() .. ' ' .. tostring(localization.followingSystem()) .. ' ' .. localization.fallback()"), "es false en");
}

TEST(LocalizationPluginTest, RefusesAFallbackWithoutItsFile) {
    test::EngineFixture missing({{"app.json", R"({"localization": {"folder": "locale", "fallback": "de"}})"}, {"content/locale/en.json", "{}"}});
    ASSERT_NE(missing.engine().getError(), nullptr);
    EXPECT_EQ(missing.engine().getError()->getMessage(), "The \"localization.fallback\" of \"app.json\" names \"de\", whose file the folder \"locale\" lacks.");
}

TEST(LocalizationPluginTest, RefusesAnEmptyFolder) {
    test::EngineFixture empty({{"app.json", R"({"localization": {"folder": "texts"}})"}, {"content/other/en.json", "{}"}});
    ASSERT_NE(empty.engine().getError(), nullptr);
    EXPECT_EQ(empty.engine().getError()->getMessage(), "The folder \"texts\" of \"localization.folder\" in \"app.json\" holds no language file. Add a file such as \"texts/en.json\".");
}

TEST(LocalizationPluginTest, ReadsTheLocalizationSectionOfAppJson) {
    const core::AppConfig config = core::AppConfig::fromJson(core::Json::parse(R"({"localization": {"folder": "./locale", "fallback": "EN", "language": "pt_br"}})"));
    EXPECT_EQ(config.localization.folder, "locale");
    EXPECT_EQ(config.localization.fallback, "en");
    EXPECT_EQ(config.localization.language, "pt-BR");
    EXPECT_EQ(config.toJson().at("localization"), core::Json::parse(R"({"folder": "locale", "fallback": "en", "language": "pt-BR"})"));
    EXPECT_EQ(core::AppConfig::fromJson(core::Json::object()).toJson().at("localization"), core::Json::object());

    // clang-format off
    const auto error = [](const char* document) {
        try {
            (void)core::AppConfig::fromJson(core::Json::parse(document));
        } catch (const std::invalid_argument& failure) {
            return std::string(failure.what());
        }
        return std::string();
    };
    // clang-format on
    EXPECT_EQ(error(R"({"localization": {"language": "Portuguese"}})"), "The \"localization.language\" value \"Portuguese\" in \"app.json\" is no language tag. Write a language such as \"en\", \"pt-BR\" or \"pt_BR\".");
    EXPECT_EQ(error(R"({"localization": {"fallback": "en"}})"), "The \"localization.fallback\" of \"app.json\" needs a \"localization.folder\" that holds its language file.");
    EXPECT_EQ(error(R"({"localization": {"default": "en"}})"), "Unknown key \"default\" in the \"localization\" section of \"app.json\".");
}

} // namespace haylen::localization
