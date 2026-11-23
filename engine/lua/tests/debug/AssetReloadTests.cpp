#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "debug/PackageEditTest.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "support/LuaFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::lua::debug {

class AssetReloadTest : public PackageEditTest {};

TEST_F(AssetReloadTest, UpdatesTexturesInPlaceAndDropsOtherAssets) {
    test::LuaFixture fixture({{"content/hero.png", toText(test::TestFiles::pngImage(4, 4, 0xFF0000FFU))}, {"content/data.json", R"({"level": 1})"}});
    haylen::assets::Manager& assets = fixture.engine().getAssets();
    const haylen::graphics::Texture hero = assets.texture("hero.png");
    EXPECT_EQ(assets.json("data.json").at("level"), 1);
    EXPECT_EQ(hero.getWidth(), 4);

    fixture.package().setFile("content/hero.png", test::TestFiles::pngImage(8, 2, 0x00FF00FFU));
    fixture.package().setFile("content/data.json", test::TestFiles::bytes(R"({"level": 2})"));
    EXPECT_EQ(assets.reload("hero.png"), (haylen::assets::Manager::Reload{.inPlace = 1}));
    EXPECT_EQ(hero.getWidth(), 8);
    EXPECT_EQ(hero.getHeight(), 2);
    EXPECT_EQ(assets.reload("data.json"), haylen::assets::Manager::Reload{});
    EXPECT_EQ(assets.json("data.json").at("level"), 2);
    EXPECT_EQ(assets.reload("missing.png"), haylen::assets::Manager::Reload{});

    fixture.package().setFile("content/hero.png", test::TestFiles::bytes("broken"));
    EXPECT_THROW((void)assets.reload("hero.png"), std::runtime_error);
}

TEST_F(AssetReloadTest, UpdatesJsonInPlaceAndAnnouncesStaleAssets) {
    test::LuaFixture fixture({{"content/data.json", R"({"level": 1})"}, {"content/icon.svg", R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 12"><path d="M0 0h24v12z"/></svg>)"}});
    // clang-format off
    fixture.runLua(R"(
        heard = {}
        local events = require('haylen.events')
        events.on('assetReloaded', function(event) heard[#heard + 1] = 'reloaded ' .. event.type .. ' ' .. event.path end)
        events.on('assetChanged', function(event) heard[#heard + 1] = 'changed ' .. event.type .. ' ' .. event.path end)
        icon = require('haylen.assets').vectorImage('icon.svg')
    )");
    // clang-format on
    haylen::assets::Manager& assets = fixture.engine().getAssets();
    const std::shared_ptr<void> held = assets.load("json", "data.json");

    fixture.package().setFile("content/data.json", test::TestFiles::bytes(R"({"level": 2})"));
    fixture.package().setFile("content/icon.svg", test::TestFiles::bytes(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 48 12"><path d="M0 0h48v12z"/></svg>)"));
    EXPECT_EQ(assets.reload("data.json"), (haylen::assets::Manager::Reload{.inPlace = 1}));
    EXPECT_EQ(assets.reload("icon.svg"), (haylen::assets::Manager::Reload{.stale = 1}));
    EXPECT_EQ(assets.json("data.json").at("level"), 2);
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat(heard, ', ')"), "reloaded json data.json, changed vectorImage icon.svg");
}

TEST_F(AssetReloadTest, RebuildsCatalogsAndThemesFromChangedFiles) {
    // clang-format off
    test::LuaFixture fixture({
        {"content/locale/en.json", R"({"greeting": "Hello"})"},
        {"content/themes/wood.json", R"({"name": "wood", "colors": {"accent": "#FF102030"}})"},
        {"source/main.lua", R"(
            localization = require('haylen.localization')
            ui = require('haylen.ui')
            localization.loadFolder('locale')
            ui.setTheme(ui.loadTheme('themes/wood.json', 'light'))
        )"},
    }, nullptr, {.development = true});
    // clang-format on
    EXPECT_EQ(fixture.lua("return localization.text('greeting')"), "Hello");

    fixture.package().setFile("content/locale/en.json", test::TestFiles::bytes(R"({"greeting": "Hi again"})"));
    fixture.package().setFile("content/themes/wood.json", test::TestFiles::bytes(R"({"name": "wood", "colors": {"accent": "#FF405060"}})"));
    const std::vector<std::string> changed{"content/locale/en.json", "content/themes/wood.json"};
    fixture.developmentSession().addChanges(changed);
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return localization.text('greeting')"), "Hi again");
    EXPECT_EQ(fixture.lua("return ui.theme() .. ' ' .. ui.themeColor('accent'):toHex()"), "wood #FF405060");
    EXPECT_FALSE(fixture.engine().isRestartRequested());

    // An editor may still be writing, so texts that are not JSON yet and a theme that does not build yet keep what the app had.
    fixture.package().setFile("content/locale/en.json", test::TestFiles::bytes("{"));
    fixture.package().setFile("content/themes/wood.json", test::TestFiles::bytes(R"({"name": "wood", "colors": {"accent": "teal"}})"));
    fixture.developmentSession().addChanges(changed);
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return localization.text('greeting')"), "Hi again");
    EXPECT_EQ(fixture.lua("return ui.themeColor('accent'):toHex()"), "#FF405060");
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_FALSE(fixture.engine().isRestartRequested());
}

} // namespace haylen::lua::debug
