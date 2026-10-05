#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>

#include <imgui_internal.h>

#include "haylen/core/Engine.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/ui/FocusNavigator.hpp"
#include "support/DrawingScene.hpp"
#include "support/RecordingEffect.hpp"
#include "support/UiFixture.hpp"

namespace haylen::ui {

namespace {

// A menu scene with a GUI of its own, which tests cover with another scene through transitions.
class SceneGuiTest : public ::testing::Test, public test::UiFixture {
  protected:
    SceneGuiTest() {
        menu = std::make_shared<test::DrawingScene>([](core::Engine&) {});
        getEngine().getScenes().push(menu);
        frames();
        gui = getUi().createGui(core::Json::parse(R"({"kind": "column", "padding": 40, "children": [{"kind": "button", "id": "play", "text": "Play"}]})"), Placement::Screen);
        getUi().mount(gui, 0, menu);
        frames(2);
    }

    // Whether a window of the UI drew this frame, with ImGui meshes or with the shapes and text of its callbacks.
    [[nodiscard]] bool hasDrawn(const char* name) {
        getUi().getBackend().makeCurrent();
        const ImGuiWindow* window = ImGui::FindWindowByName(name);
        // clang-format off
        return window != nullptr && window->Active && std::ranges::any_of(window->DrawList->CmdBuffer, [](const ImDrawCmd& command) {
            return command.ElemCount > 0 || command.UserCallback != nullptr;
        });
        // clang-format on
    }

    void push(float switchProgress, float exitProgress) {
        effect = std::make_shared<test::RecordingEffect>(switchProgress, exitProgress);
        getEngine().getScenes().push(std::make_shared<test::DrawingScene>([](core::Engine&) {}), {.transition = {.duration = 1.0F, .effect = effect}});
    }

    std::shared_ptr<test::DrawingScene> menu;
    std::shared_ptr<Gui> gui;
    std::shared_ptr<test::RecordingEffect> effect;
};

} // namespace

TEST_F(SceneGuiTest, LeavesWithItsSceneThroughAnEffectThatShowsBothScenes) {
    ASSERT_TRUE(hasDrawn("##haylen-guis"));

    // The menu leaves into the outgoing image with its GUI, and the image of the scene that arrives never shows the GUI of the menu.
    push(0.0F, 1.0F);
    frames(10);
    ASSERT_TRUE(getEngine().getScenes().isTransitioning());
    EXPECT_TRUE(hasDrawn("##haylen-leaving"));
    EXPECT_FALSE(hasDrawn("##haylen-guis"));

    // The GUI takes no input while it leaves.
    clearEvents();
    click(*gui, "play");
    EXPECT_TRUE(getEventNames().empty());

    frames(60);
    ASSERT_FALSE(getEngine().getScenes().isTransitioning());
    EXPECT_FALSE(hasDrawn("##haylen-leaving"));
    EXPECT_FALSE(hasDrawn("##haylen-guis"));
    EXPECT_TRUE(getUi().isMounted(*gui));
}

TEST_F(SceneGuiTest, HidesTheGuiOfACoveredSceneUntilItShowsAgain) {
    push(0.5F, 0.5F);
    frames(70);
    ASSERT_FALSE(getEngine().getScenes().isTransitioning());
    EXPECT_FALSE(hasDrawn("##haylen-guis"));
    clearEvents();
    click(*gui, "play");
    EXPECT_TRUE(getEventNames().empty());

    // Popped through an effect that covers the screen, the menu shows its GUI in the image the effect reveals.
    getEngine().getScenes().pop({.duration = 1.0F, .effect = effect});
    frames(40);
    ASSERT_TRUE(getEngine().getScenes().isTransitioning());
    EXPECT_TRUE(hasDrawn("##haylen-guis"));
    frames(30);
    click(*gui, "play");
    EXPECT_EQ(findLastEvent("click").id, "play");
}

TEST_F(SceneGuiTest, GivesTheFocusBackOnceItsSceneShowsAgain) {
    gui->command(getUi().getContext(), "play", "focus", core::Json::object());
    frames(2);
    ASSERT_TRUE(isFocused(*gui, "play"));
    getEngine().getScenes().push(std::make_shared<test::DrawingScene>([](core::Engine&) {}));
    frames(3);
    EXPECT_EQ(getUi().getFocus().getOwner(), FocusNavigator::Owner::None);
    getEngine().getScenes().pop();
    frames(3);
    EXPECT_TRUE(isFocused(*gui, "play"));

    // A GUI that leaves through an effect that shows both scenes comes back the same way.
    push(0.0F, 1.0F);
    frames(70);
    getEngine().getScenes().pop({.duration = 1.0F, .effect = effect});
    frames(70);
    EXPECT_TRUE(isFocused(*gui, "play"));

    // A focus that moved elsewhere while the GUI was away stays there.
    auto other = getUi().createGui(core::Json::parse(R"({"kind": "button", "id": "other", "text": "Other"})"), Placement::Screen);
    getUi().mount(other);
    getEngine().getScenes().push(std::make_shared<test::DrawingScene>([](core::Engine&) {}));
    frames(3);
    other->command(getUi().getContext(), "other", "focus", core::Json::object());
    frames(2);
    getEngine().getScenes().pop();
    frames(3);
    EXPECT_TRUE(isFocused(*other, "other"));
}

TEST_F(SceneGuiTest, DrawsAGuiWithoutASceneAboveEveryView) {
    auto hud = getUi().createGui(core::Json::parse(R"({"kind": "button", "id": "coins", "text": "Coins 12"})"), Placement::Screen);
    getUi().mount(hud);
    push(0.0F, 1.0F);
    frames(10);
    EXPECT_TRUE(hasDrawn("##haylen-guis"));
    EXPECT_TRUE(hasDrawn("##haylen-leaving"));
}

TEST_F(SceneGuiTest, BindsAGuiToTheSceneThatOwnsItInLua) {
    // clang-format off
    getFixture().runLua(R"(
        local scene = require('haylen.scene')
        local ui = require('haylen.ui')
        local Menu = {}
        function Menu:enter() self.gui = ui.mount(ui.button{id = 'start', text = 'Start'}, {owner = self}) end
        menuScene = setmetatable({}, {__index = Menu})
        scene.push(menuScene)
    )");
    // clang-format on
    frames(3);
    EXPECT_TRUE(hasDrawn("##haylen-guis"));
    getFixture().runLua("require('haylen.scene').push({})");
    frames(3);
    EXPECT_EQ(getFixture().lua("return tostring(menuScene.gui.mounted)"), "true");
    EXPECT_FALSE(hasDrawn("##haylen-guis"));
}

} // namespace haylen::ui
