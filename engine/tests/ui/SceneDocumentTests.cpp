#include <gtest/gtest.h>

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

// A menu scene with a document of its own, which tests cover with another scene through transitions.
class SceneDocumentTest : public ::testing::Test, public test::UiFixture {
  protected:
    SceneDocumentTest() {
        menu = std::make_shared<test::DrawingScene>([](core::Engine&) {});
        getEngine().getScenes().push(menu);
        frames();
        document = getUi().createDocument(core::Json::parse(R"({"kind": "column", "padding": 40, "children": [{"kind": "button", "id": "play", "text": "Play"}]})"), Placement::Screen);
        getUi().mount(document, 0, menu);
        frames(2);
    }

    // The number of vertices a window of the UI drew this frame, or nothing when it did not draw.
    [[nodiscard]] int getVertices(const char* name) {
        getUi().getBackend().makeCurrent();
        const ImGuiWindow* window = ImGui::FindWindowByName(name);
        return window != nullptr && window->Active ? window->DrawList->VtxBuffer.Size : 0;
    }

    void push(float switchProgress, float exitProgress) {
        effect = std::make_shared<test::RecordingEffect>(switchProgress, exitProgress);
        getEngine().getScenes().push(std::make_shared<test::DrawingScene>([](core::Engine&) {}), {.transition = {.duration = 1.0F, .effect = effect}});
    }

    std::shared_ptr<test::DrawingScene> menu;
    std::shared_ptr<Document> document;
    std::shared_ptr<test::RecordingEffect> effect;
};

} // namespace

TEST_F(SceneDocumentTest, LeavesWithItsSceneThroughAnEffectThatShowsBothScenes) {
    ASSERT_GT(getVertices("##haylen-documents"), 0);

    // The menu leaves into the outgoing image with its document, and the image of the scene that arrives never shows the document of the menu.
    push(0.0F, 1.0F);
    frames(10);
    ASSERT_TRUE(getEngine().getScenes().isTransitioning());
    EXPECT_GT(getVertices("##haylen-leaving"), 0);
    EXPECT_EQ(getVertices("##haylen-documents"), 0);

    // The document takes no input while it leaves.
    clearEvents();
    click(*document, "play");
    EXPECT_TRUE(getEventNames().empty());

    frames(60);
    ASSERT_FALSE(getEngine().getScenes().isTransitioning());
    EXPECT_EQ(getVertices("##haylen-leaving"), 0);
    EXPECT_EQ(getVertices("##haylen-documents"), 0);
    EXPECT_TRUE(getUi().isMounted(*document));
}

TEST_F(SceneDocumentTest, HidesTheDocumentOfACoveredSceneUntilItShowsAgain) {
    push(0.5F, 0.5F);
    frames(70);
    ASSERT_FALSE(getEngine().getScenes().isTransitioning());
    EXPECT_EQ(getVertices("##haylen-documents"), 0);
    clearEvents();
    click(*document, "play");
    EXPECT_TRUE(getEventNames().empty());

    // Popped through an effect that covers the screen, the menu shows its document in the image the effect reveals.
    getEngine().getScenes().pop({.duration = 1.0F, .effect = effect});
    frames(40);
    ASSERT_TRUE(getEngine().getScenes().isTransitioning());
    EXPECT_GT(getVertices("##haylen-documents"), 0);
    frames(30);
    click(*document, "play");
    EXPECT_EQ(findLastEvent("click").id, "play");
}

TEST_F(SceneDocumentTest, GivesTheFocusBackOnceItsSceneShowsAgain) {
    document->command(getUi().getContext(), "play", "focus", core::Json::object());
    frames(2);
    ASSERT_TRUE(isFocused(*document, "play"));
    getEngine().getScenes().push(std::make_shared<test::DrawingScene>([](core::Engine&) {}));
    frames(3);
    EXPECT_EQ(getUi().getFocus().getOwner(), FocusNavigator::Owner::None);
    getEngine().getScenes().pop();
    frames(3);
    EXPECT_TRUE(isFocused(*document, "play"));

    // A document that leaves through an effect that shows both scenes comes back the same way.
    push(0.0F, 1.0F);
    frames(70);
    getEngine().getScenes().pop({.duration = 1.0F, .effect = effect});
    frames(70);
    EXPECT_TRUE(isFocused(*document, "play"));

    // A focus that moved elsewhere while the document was away stays there.
    auto other = getUi().createDocument(core::Json::parse(R"({"kind": "button", "id": "other", "text": "Other"})"), Placement::Screen);
    getUi().mount(other);
    getEngine().getScenes().push(std::make_shared<test::DrawingScene>([](core::Engine&) {}));
    frames(3);
    other->command(getUi().getContext(), "other", "focus", core::Json::object());
    frames(2);
    getEngine().getScenes().pop();
    frames(3);
    EXPECT_TRUE(isFocused(*other, "other"));
}

TEST_F(SceneDocumentTest, DrawsADocumentWithoutASceneAboveEveryView) {
    auto hud = getUi().createDocument(core::Json::parse(R"({"kind": "button", "id": "coins", "text": "Coins 12"})"), Placement::Screen);
    getUi().mount(hud);
    push(0.0F, 1.0F);
    frames(10);
    EXPECT_GT(getVertices("##haylen-documents"), 0);
    EXPECT_GT(getVertices("##haylen-leaving"), 0);
}

TEST_F(SceneDocumentTest, BindsADocumentToTheSceneThatOwnsItInLua) {
    // clang-format off
    getFixture().runLua(R"(
        local scene = require('haylen.scene')
        local ui = require('haylen.ui')
        local Menu = {}
        function Menu:enter() self.document = ui.mount(ui.button{id = 'start', text = 'Start'}, {owner = self}) end
        menuScene = setmetatable({}, {__index = Menu})
        scene.push(menuScene)
    )");
    // clang-format on
    frames(3);
    EXPECT_GT(getVertices("##haylen-documents"), 0);
    getFixture().runLua("require('haylen.scene').push({})");
    frames(3);
    EXPECT_EQ(getFixture().lua("return tostring(menuScene.document.mounted)"), "true");
    EXPECT_EQ(getVertices("##haylen-documents"), 0);
}

} // namespace haylen::ui
