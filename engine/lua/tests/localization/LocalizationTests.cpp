#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/core/AppConfig.hpp"
#include "support/LuaFixture.hpp"

namespace haylen::lua::localization {

TEST(LocalizationLuaTest, LoadsLanguageFoldersFromThePackage) {
    // clang-format off
    test::LuaFixture fixture({
        {"content/i18n/en.json", R"({"hud": {"day": "Day {day}", "wood": {"one": "{count} log", "other": "{count} logs"}}})"},
        {"content/i18n/pt_BR.json", R"({"hud": {"day": "Dia {day}"}})"},
        {"content/i18n/notes.txt", "ignored"},
        {"content/broken/en.json", "{\n  \"hud\": @\n}"},
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
    EXPECT_NE(fixture.lua("localization.loadFolder('broken')").find("The file \"broken/en.json\" is not valid JSON. Check line 2, column 10."), std::string::npos);
    EXPECT_NE(fixture.lua("localization.setLanguage('pt BR')").find("The language tag \"pt BR\" is not valid."), std::string::npos);
    EXPECT_NE(fixture.lua("localization.setFallback('xx')").find("No localization table was added for \"xx\""), std::string::npos);
    EXPECT_EQ(fixture.lua("return localization.normalizeTag('zh_hant_tw')"), "zh-Hant-TW");
    EXPECT_NE(fixture.lua("localization.add('de', {menu = {count = 3}})").find("menu.count"), std::string::npos);
}

TEST(LocalizationLuaTest, RefusesFilesNotNamedAfterALanguage) {
    test::LuaFixture fixture({{"content/i18n/english.json", "{}"}});
    fixture.runLua("localization = require('haylen.localization')");
    EXPECT_NE(fixture.lua("localization.loadFolder('i18n')").find("The localization file \"i18n/english.json\" is not named after a language tag."), std::string::npos);
}

// A folder that downloads, such as the dialogue of an expansion, loads only asynchronously, since a synchronous load never waits for a download.
TEST(LocalizationLuaTest, LoadsAFolderThatDownloadsOnlyAsynchronously) {
    // clang-format off
    test::LuaFixture fixture({
        {"haylen.json", R"({"content": {"delivery": [{"match": "expansion/**", "mode": "onDemand"}]}})"},
        {"content/expansion/dialogue/en.json", R"({"quest": "Find the key"})"},
        {"content/expansion/broken/en.json", "{"},
    });
    fixture.runLua(R"(
        localization = require('haylen.localization')
        require('async').spawn(function()
            loaded = table.concat(localization.loadFolderAsync('expansion/dialogue'):await(), ',')
            local languages, message = localization.loadFolderAsync('expansion/broken'):await()
            refused = message
        end)
    )");
    // clang-format on

    EXPECT_NE(fixture.lua("localization.loadFolder('expansion/dialogue')").find("The localization file \"expansion/dialogue/en.json\" is not on the device yet"), std::string::npos);
    ASSERT_TRUE(fixture.frameUntil([&fixture] { return fixture.lua("return tostring(refused)") != "nil"; }));
    EXPECT_EQ(fixture.lua("return loaded .. ' ' .. localization.text('quest')"), "en Find the key");
    EXPECT_NE(fixture.lua("return refused").find("is not valid JSON"), std::string::npos);
}

// The section `localization` of haylen.json loads the languages before any script runs, the system picks among them, and every change of the current language reaches the app once, at the start of the next frame.
TEST(LocalizationPluginTest, FollowsTheSystemAndAnnouncesEveryChangeOfLanguage) {
    // clang-format off
    test::LuaFixture fixture({
        {"haylen.json", R"({"localization": {"folder": "locale", "fallback": "en"}})"},
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

// A language that haylen.json names is the choice of the app from the start.
TEST(LocalizationPluginTest, StartsInTheLanguageOfHaylenJson) {
    // clang-format off
    test::LuaFixture fixture({
        {"haylen.json", R"({"localization": {"folder": "locale", "language": "es_419"}})"},
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
    test::LuaFixture missing({{"haylen.json", R"({"localization": {"folder": "locale", "fallback": "de"}})"}, {"content/locale/en.json", "{}"}});
    ASSERT_NE(missing.engine().getError(), nullptr);
    EXPECT_EQ(missing.engine().getError()->getMessage(), "The \"localization.fallback\" of \"haylen.json\" names \"de\", whose file the folder \"locale\" lacks.");
}

TEST(LocalizationPluginTest, RefusesAnEmptyFolder) {
    test::LuaFixture empty({{"haylen.json", R"({"localization": {"folder": "texts"}})"}, {"content/other/en.json", "{}"}});
    ASSERT_NE(empty.engine().getError(), nullptr);
    EXPECT_EQ(empty.engine().getError()->getMessage(), "The folder \"texts\" of \"localization.folder\" in \"haylen.json\" holds no language file. Add a file such as \"texts/en.json\".");
}

TEST(LocalizationPluginTest, ReadsTheLocalizationSectionOfHaylenJson) {
    const haylen::core::AppConfig config = haylen::core::AppConfig::fromJson(haylen::core::Json::parse(R"({"localization": {"folder": "./locale", "fallback": "EN", "language": "pt_br"}})"));
    EXPECT_EQ(config.localization.folder, "locale");
    EXPECT_EQ(config.localization.fallback, "en");
    EXPECT_EQ(config.localization.language, "pt-BR");
    EXPECT_EQ(config.toJson().at("localization"), haylen::core::Json::parse(R"({"folder": "locale", "fallback": "en", "language": "pt-BR"})"));
    EXPECT_EQ(haylen::core::AppConfig::fromJson(haylen::core::Json::object()).toJson().at("localization"), haylen::core::Json::object());

    // clang-format off
    const auto error = [](const char* document) {
        try {
            (void)haylen::core::AppConfig::fromJson(haylen::core::Json::parse(document));
        } catch (const std::invalid_argument& failure) {
            return std::string(failure.what());
        }

        return std::string();
    };
    // clang-format on
    EXPECT_EQ(error(R"({"localization": {"language": "Portuguese"}})"), "The \"localization.language\" value \"Portuguese\" in \"haylen.json\" is no language tag. Write a language such as \"en\", \"pt-BR\" or \"pt_BR\".");
    EXPECT_EQ(error(R"({"localization": {"fallback": "en"}})"), "The \"localization.fallback\" of \"haylen.json\" needs a \"localization.folder\" that holds its language file.");
    EXPECT_EQ(error(R"({"localization": {"default": "en"}})"), "Unknown key \"default\" in the \"localization\" section of \"haylen.json\".");
}

} // namespace haylen::lua::localization
