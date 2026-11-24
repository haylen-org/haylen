#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/localization/Catalog.hpp"

namespace haylen::localization {

class CatalogTest : public ::testing::Test {
  protected:
    [[nodiscard]] static Catalog makeEnglish() {
        Catalog catalog;
        // clang-format off
        catalog.add("en", haylen::core::Json::parse(R"({
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
    catalog.add("en", haylen::core::Json::parse(R"({"story": "[b]Once[/b] upon {{a}} time"})"));
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
    catalog.add("pt-BR", haylen::core::Json::parse(R"({"menu": {"play": "Jogar"}})"));
    catalog.add("en", haylen::core::Json::parse(R"({"menu": {"settings": "Settings"}})"));
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
    catalog.add("en", haylen::core::Json::parse(R"({"note": "{{literal}} {missing} {flag} {n} {oops", "odd": {"several": "a", "other": "b"}})"));
    EXPECT_EQ(catalog.getText("note", {{"flag", true}, {"n", 3}}), "{literal} {missing} true 3 {oops");
    EXPECT_EQ(catalog.getText("odd.several"), "a");
    EXPECT_EQ(catalog.getText("odd.other"), "b");
    EXPECT_FALSE(catalog.has("odd"));

    EXPECT_THROW(catalog.add("", haylen::core::Json::object()), std::invalid_argument);
    EXPECT_THROW(catalog.add("en", haylen::core::Json::array()), std::invalid_argument);
    EXPECT_THROW(catalog.add("en", haylen::core::Json::parse(R"({"menu": {"count": 3}})")), std::invalid_argument);
    EXPECT_THROW((void)catalog.getText("note", haylen::core::Json::array()), std::invalid_argument);
}

TEST_F(CatalogTest, RefusesTablesThatNestWithoutEnd) {
    constexpr int kLevels = 200000;
    std::string text;
    for (int level = 0; level < kLevels; ++level) {
        text += R"({"a":)";
    }

    text += R"("deep")";
    text.append(kLevels, '}');

    Catalog catalog = makeEnglish();
    try {
        catalog.add("en", haylen::core::Json::parse(text));
        ADD_FAILURE() << "A table that nests without end must be refused.";
    } catch (const std::invalid_argument& error) {
        EXPECT_TRUE(std::string(error.what()).ends_with("\" nests deeper than 32 levels.")) << error.what();
    }

    EXPECT_EQ(catalog.getText("menu.play"), "Play");
}

TEST_F(CatalogTest, FormatsTextsOutsideTheCatalogLikeItsOwn) {
    EXPECT_EQ(Catalog::format("You carry {gold} coins, {{gold}} {missing}.", {{"gold", 30}}), "You carry 30 coins, {gold} {missing}.");
    EXPECT_THROW((void)Catalog::format("{gold}", haylen::core::Json::array()), std::invalid_argument);
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

// The tag itself wins, then the tag with the script that its region implies and without its last subtags, then the first language added of the same language whose script agrees.
TEST_F(CatalogTest, MatchesLanguageTagsInOrder) {
    Catalog catalog;
    for (const char* language : {"en", "pt", "pt-PT", "es-MX", "es-AR", "zh-Hans", "zh-Hant-TW"}) {
        catalog.add(language, haylen::core::Json::object());
    }

    EXPECT_EQ(catalog.findBestMatch("PT_pt"), "pt-PT");
    EXPECT_EQ(catalog.findBestMatch("pt-BR"), "pt");
    EXPECT_EQ(catalog.findBestMatch("en-Latn-US"), "en");
    EXPECT_EQ(catalog.findBestMatch("es"), "es-MX");
    EXPECT_EQ(catalog.findBestMatch("es-CL"), "es-MX");
    EXPECT_EQ(catalog.findBestMatch("zh-Hans-CN"), "zh-Hans");
    EXPECT_EQ(catalog.findBestMatch("zh-Hant-HK"), "zh-Hant-TW");
    EXPECT_EQ(catalog.findBestMatch("zh-TW"), "zh-Hant-TW");
    EXPECT_EQ(catalog.findBestMatch("zh-HK"), "zh-Hant-TW");
    EXPECT_EQ(catalog.findBestMatch("zh-MO"), "zh-Hant-TW");
    EXPECT_EQ(catalog.findBestMatch("zh-CN"), "zh-Hans");
    EXPECT_EQ(catalog.findBestMatch("zh"), "zh-Hans");
    EXPECT_EQ(catalog.findBestMatch("de-DE"), std::nullopt);
    EXPECT_THROW((void)catalog.findBestMatch("pt BR"), std::invalid_argument);
}

// A language that the app writes with a region alone has the script of its region too, so a device in traditional Chinese never gets the simplified one.
TEST_F(CatalogTest, MatchesTheScriptThatARegionImplies) {
    Catalog catalog;
    for (const char* language : {"zh-CN", "zh-TW"}) {
        catalog.add(language, haylen::core::Json::object());
    }

    EXPECT_EQ(catalog.findBestMatch("zh-Hant-HK"), "zh-TW");
    EXPECT_EQ(catalog.findBestMatch("zh-HK"), "zh-TW");
    EXPECT_EQ(catalog.findBestMatch("zh-Hans-SG"), "zh-CN");
    EXPECT_EQ(catalog.findBestMatch("zh"), "zh-CN");
}

// Without a choice of the app, the first language of the system with a close language wins, then the fallback, which is the first language added until the app names another.
TEST_F(CatalogTest, FollowsTheLanguagesOfTheSystem) {
    Catalog catalog;
    catalog.add("ja", haylen::core::Json::object());
    catalog.add("en", haylen::core::Json::object());
    catalog.add("pt_BR", haylen::core::Json::object());
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
    catalog.add("de-DE", haylen::core::Json::object());
    EXPECT_EQ(catalog.getLanguage(), "de-DE");
    EXPECT_THROW(catalog.setSystemLanguages(std::vector<std::string>{"not a tag"}), std::invalid_argument);
}

// The choice of the app outlives changes of the system and languages added later, and falls back like the system does.
TEST_F(CatalogTest, KeepsTheChoiceOfTheAppUntilItFollowsTheSystemAgain) {
    Catalog catalog;
    catalog.setLanguage("PT_br");
    EXPECT_FALSE(catalog.isFollowingSystem());
    EXPECT_EQ(catalog.getLanguage(), "");
    catalog.add("en", haylen::core::Json::object());
    EXPECT_EQ(catalog.getLanguage(), "en");
    catalog.add("pt-BR", haylen::core::Json::object());
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
    catalog.add("ar", haylen::core::Json::parse(R"({"@direction": "rightToLeft", "menu": {"play": "العب"}})"));
    catalog.add("ar", haylen::core::Json::parse(R"({"menu": {"quit": "خروج"}})"));
    EXPECT_EQ(catalog.getDirection("en"), haylen::text::Direction::LeftToRight);
    EXPECT_EQ(catalog.getDirection("ar"), haylen::text::Direction::RightToLeft);
    catalog.setLanguage("ar");
    EXPECT_EQ(catalog.getText("menu.play"), "العب");
    EXPECT_FALSE(catalog.has("@direction"));
    EXPECT_THROW(catalog.add("he", haylen::core::Json::parse(R"({"@direction": "up"})")), std::invalid_argument);
    EXPECT_THROW((void)catalog.getDirection("xx"), std::invalid_argument);
}

} // namespace haylen::localization
