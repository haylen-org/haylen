#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <string>

#include <imgui.h>

#include "haylen/core/Engine.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/ui/Backend.hpp"
#include "support/AllocationTracker.hpp"
#include "support/LuaFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::lua::ui {

// Runs the Lua UI on packages that hold fonts of the test data, which the families of the roles of a theme draw with.
class UiLuaFontTest : public ::testing::Test {
  protected:
    [[nodiscard]] static std::string readTestFont(const std::string& name) {
        return test::TestFiles::readText(std::filesystem::path(HAYLEN_TEST_FONTS) / name);
    }
};

// Labels and every other text component draw with the faces of the family of their role, bold where the role asks for it, and take the characters the face lacks from the fallbacks, while rich text of a role lines up with them.
TEST_F(UiLuaFontTest, DrawsTextComponentsWithTheFacesAndFallbacksOfAFamily) {
    test::LuaFixture fixture({{"content/fonts/cjk.ttf", readTestFont("mplus-1p-regular.ttf")}, {"content/fonts/bold.ttf", readTestFont("crimson-text-bold.ttf")}});
    // clang-format off
    fixture.runLua(R"(
        assets = require('haylen.assets')
        graphics = require('haylen.graphics')
        graphics2d = require('haylen.graphics2d')
        ui = require('haylen.ui')
        ui.addFont('story', graphics.newFontFamily({regular = graphics2d.defaultFont(), bold = assets.font('fonts/bold.ttf'), fallbacks = {assets.font('fonts/cjk.ttf')}}))
        ui.setTheme(ui.addTheme({name = 'story', fonts = {body = {font = 'story'}, heading = {font = 'story', bold = true}}}))
        screen = ui.mount(ui.column{
            ui.label{id = 'plain', text = 'Harbor lights', align = 'start'},
            ui.richText{id = 'rich', text = 'Harbor lights', align = 'start'},
            ui.label{id = 'japanese', text = '日本語', align = 'start'},
            ui.label{id = 'heading', text = 'Harbor lights', font = 'heading', align = 'start'},
        }, {placement = 'screen'})
    )");
    // clang-format on
    fixture.frames(2);
    EXPECT_EQ(fixture.lua("local role = ui.themeFont('heading') return role.font .. ' ' .. tostring(role.bold) .. ' ' .. tostring(role.italic)"), "story true false");
    EXPECT_NEAR(std::stof(fixture.lua("return screen:bounds('plain').width - screen:bounds('rich').width")), 0.0F, 2.0F);

    // A Japanese character comes from the fallback and advances by one em, the size of the body.
    EXPECT_NEAR(std::stof(fixture.lua("return screen:bounds('japanese').width")), 3.0F * 26.0F, 1.0F);

    // The bold face of the heading is another font, so the same text takes another width than in the regular face.
    const std::string bold = fixture.lua("return screen:bounds('heading').width");
    fixture.runLua("ui.setTheme(ui.addTheme({name = 'plain', fonts = {heading = {font = 'story'}}}))");
    fixture.frames(2);
    EXPECT_NE(fixture.lua("return screen:bounds('heading').width"), bold);
    EXPECT_NE(fixture.lua("ui.addTheme({name = 'odd', fonts = {body = {bold = 'yes'}}})").find("The theme font \"body\" must set \"bold\" to \"true\" or \"false\"."), std::string::npos);

    // A font that is neither registered nor the default has no family.
    haylen::plugins::UiPlugin& plugin = fixture.engine().getPlugin<haylen::plugins::UiPlugin>();
    EXPECT_NE(plugin.getFontFamily(fixture.engine(), "story"), nullptr);
    EXPECT_EQ(plugin.getFontFamily(fixture.engine(), "missing"), nullptr);
}

// Labels, fields and rich text take color emoji from the fallback of their family, which draw as rasters that frames reuse, so a GUI with emoji allocates nothing more once it has drawn.
TEST_F(UiLuaFontTest, DrawsColorEmojiInComponentsWithoutAllocating) {
    test::LuaFixture fixture({{"content/fonts/emoji.ttf", readTestFont("noto-color-emoji-regular.ttf")}});
    // clang-format off
    fixture.runLua(R"(
        assets = require('haylen.assets')
        graphics = require('haylen.graphics')
        graphics2d = require('haylen.graphics2d')
        ui = require('haylen.ui')
        ui.addFont('chat', graphics.newFontFamily({regular = graphics2d.defaultFont(), fallbacks = {assets.font('fonts/emoji.ttf')}}))
        ui.setTheme(ui.addTheme({name = 'chat', fonts = {body = {font = 'chat'}}}))
    )");
    // clang-format on
    // clang-format off
    const auto measure = [&fixture] {
        fixture.frames(5);
        const test::AllocationTracker tracker;
        fixture.frames(10);
        return tracker.getTotal();
    };
    // clang-format on
    const std::size_t empty = measure();

    // clang-format off
    fixture.runLua(R"(
        screen = ui.mount(ui.column{
            ui.label{id = 'party', text = 'Party \u{1F389}', align = 'start'},
            ui.label{id = 'plain', text = 'Party ', align = 'start'},
            ui.textField{id = 'message', value = 'Ship it \u{1F680}\u{1F469}\u{200D}\u{1F4BB}'},
            ui.richText{id = 'rich', text = '[b]Hot[/b] \u{1F525} [color=#FF0000]love[/color] \u{2764}\u{FE0F}'},
        }, {placement = 'screen'})
    )");
    // clang-format on
    EXPECT_EQ(measure(), empty);
    EXPECT_EQ(fixture.engine().getError(), nullptr);

    // The emoji takes the room of its advance in the fallback, one em and a quarter of the body.
    EXPECT_GT(std::stof(fixture.lua("return screen:bounds('party').width - screen:bounds('plain').width")), 26.0F);
}

// A size names the em square of every font, so a Japanese character that falls back advances by the size of its role whatever the ascent and descent of the face, and the lines of a face that reaches higher and lower grow taller instead.
TEST_F(UiLuaFontTest, SizesFontRolesByTheirEmSquare) {
    test::LuaFixture fixture({{"content/fonts/cjk.ttf", readTestFont("mplus-1p-regular.ttf")}, {"content/fonts/serif.ttf", readTestFont("crimson-text-regular.ttf")}});
    // clang-format off
    fixture.runLua(R"(
        assets = require('haylen.assets')
        graphics = require('haylen.graphics')
        graphics2d = require('haylen.graphics2d')
        ui = require('haylen.ui')
        local cjk = assets.font('fonts/cjk.ttf')
        ui.addFont('sans', graphics.newFontFamily({regular = graphics2d.defaultFont(), fallbacks = {cjk}}))
        ui.addFont('serif', graphics.newFontFamily({regular = assets.font('fonts/serif.ttf'), fallbacks = {cjk}}))
        ui.setTheme(ui.addTheme({name = 'pair', fonts = {body = {font = 'sans', size = 24}, caption = {font = 'serif', size = 24}}}))
        screen = ui.mount(ui.column{
            ui.label{id = 'sansJapanese', text = '日本語', align = 'start'},
            ui.label{id = 'serifJapanese', text = '日本語', font = 'caption', align = 'start'},
            ui.label{id = 'sans', text = 'Harbor', align = 'start'},
            ui.label{id = 'serif', text = 'Harbor', font = 'caption', align = 'start'},
        }, {placement = 'screen'})
    )");
    // clang-format on
    fixture.frames(2);
    EXPECT_NEAR(std::stof(fixture.lua("return screen:bounds('sansJapanese').width")), 72.0F, 1.0F);
    EXPECT_NEAR(std::stof(fixture.lua("return screen:bounds('serifJapanese').width")), 72.0F, 1.0F);

    // The default font spans 2400 of its 2048 units to the em from ascent to descent, and Crimson Text 1331 of its 1024, both without a line gap.
    EXPECT_NEAR(std::stof(fixture.lua("return screen:bounds('sans').height")), 24.0F * 2400.0F / 2048.0F, 0.01F);
    EXPECT_NEAR(std::stof(fixture.lua("return screen:bounds('serif').height")), 24.0F * 1331.0F / 1024.0F, 0.01F);
    EXPECT_NEAR(std::stof(fixture.lua("return ui.themeFont('caption').lineHeight")), 24.0F * 1331.0F / 1024.0F, 0.01F);
    EXPECT_EQ(fixture.lua("return ui.themeFont('caption').size"), "24.0");

    // The windows of ImGui draw the body at its size too, which ImGui measures from ascent to descent.
    haylen::ui::Backend& backend = fixture.engine().getPlugin<haylen::plugins::UiPlugin>().getBackend();
    backend.makeCurrent();
    EXPECT_NEAR(ImGui::GetStyle().FontSizeBase, 24.0F * 2400.0F / 2048.0F, 0.01F);
}

} // namespace haylen::lua::ui
