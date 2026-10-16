#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/math/Geometry.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/ui/Gui.hpp"
#include "haylen/ui/Theme.hpp"
#include "support/UiFixture.hpp"

namespace haylen::ui {

namespace {

class OverlayTest : public ::testing::Test, public test::UiFixture {
  protected:
    // Runs a frame and returns the blocks of text the renderer drew, which are the messages of the toasts in these tests.
    [[nodiscard]] std::vector<math::Rect> drawMessages() {
        std::vector<math::Rect> blocks;
        graphics2d::Renderer& renderer = getEngine().getRenderer2D();
        // clang-format off
        const std::uint64_t overlay = renderer.addCanvasOverlay([&blocks](graphics2d::Renderer& drawing) {
            drawing.visitDrawn([&blocks](const graphics2d::Renderer::Drawn& drawn) {
                if (drawn.text) {
                    blocks.push_back(math::Geometry::bounds(drawn.corners));
                }
            });
        });
        // clang-format on
        frames();
        renderer.removeCanvasOverlay(overlay);
        return blocks;
    }

    // The alpha of the first vertex the window of an open dialog drew, which is the corner of its backdrop, or nothing once the dialog is closed.
    [[nodiscard]] std::optional<float> getBackdropAlpha() {
        getUi().getBackend().makeCurrent();
        const ImGuiContext& state = *GImGui;
        if (state.OpenPopupStack.Size == 0 || state.OpenPopupStack[0].Window == nullptr || state.OpenPopupStack[0].Window->DrawList->VtxBuffer.Size == 0) {
            return std::nullopt;
        }
        return static_cast<float>((state.OpenPopupStack[0].Window->DrawList->VtxBuffer[0].col >> IM_COL32_A_SHIFT) & 0xFFU) / 255.0F;
    }

    [[nodiscard]] int getTransitionFrames() {
        return static_cast<int>(getUi().getTheme().getMetric(Theme::Metric::TransitionDuration) * 60.0F) + 2;
    }
};

} // namespace

TEST_F(OverlayTest, StacksToastsThatShowTogetherWithoutCoveringThem) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "children": [
        {"kind": "toast", "id": "first", "text": "The first notice", "duration": 0},
        {"kind": "toast", "id": "second", "text": "The second notice", "duration": 0},
        {"kind": "toast", "id": "bottom", "text": "At the bottom", "position": "bottomEnd", "duration": 0}
    ]})");
    // clang-format on
    gui->set("first", {{"open", true}});
    frames();
    gui->set("second", {{"open", true}});
    gui->set("bottom", {{"open", true}});
    frames(getTransitionFrames() * 3);

    const math::Rect first = getBounds(*gui, "first");
    const math::Rect second = getBounds(*gui, "second");
    const math::Rect bottom = getBounds(*gui, "bottom");
    EXPECT_GE(second.y, first.getBottom());
    EXPECT_NEAR(second.getCenter().x, first.getCenter().x, 1.0F);
    EXPECT_GT(bottom.y, second.getBottom());
    EXPECT_GT(bottom.x, first.x);

    // Once the first notice leaves, the second one moves up into its place.
    gui->set("first", {{"open", false}});
    frames(getTransitionFrames() * 3);
    EXPECT_EQ(getBounds(*gui, "second").y, first.y);
    EXPECT_EQ(getLastEvent().name, "dismiss");
    EXPECT_EQ(getLastEvent().id, "first");
}

TEST_F(OverlayTest, QueuesNoticesBeyondTheLimitOfTheStack) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "toast", "id": "news", "duration": 0.5}]})");
    for (const char* message : {"One", "Two", "Three", "Four"}) {
        gui->command(getUi().getContext(), "news", "show", {{"text", message}});
    }
    frames(getTransitionFrames());
    EXPECT_EQ(drawMessages().size(), 3U);

    // The fourth notice waits until the first ones leave, and its own time starts once it shows.
    frames(40);
    EXPECT_EQ(drawMessages().size(), 1U);
    frames(40);
    EXPECT_TRUE(drawMessages().empty());

    EXPECT_THROW(gui->command(getUi().getContext(), "news", "show", {{"color", "red"}}), std::invalid_argument);
    EXPECT_THROW(gui->command(getUi().getContext(), "news", "show", {{"duration", -1}}), std::invalid_argument);
    EXPECT_THROW(gui->command(getUi().getContext(), "news", "show", "Hi"), std::invalid_argument);
}

TEST_F(OverlayTest, FadesADialogAndItsBackdropInAndOutTogether) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "dialog", "id": "quit", "title": "Quit?", "buttons": [{"id": "yes", "text": "Quit"}]}]})");
    gui->set("quit", {{"open", true}});
    frames();
    const std::optional<float> opening = getBackdropAlpha();
    ASSERT_TRUE(opening.has_value());
    EXPECT_GT(*opening, 0.0F);
    EXPECT_LT(*opening, 0.5F);

    // The backdrop reaches the overlay color of the theme within the transition, together with the dialog.
    frames(getTransitionFrames());
    EXPECT_NEAR(*getBackdropAlpha(), getUi().getTheme().getColor(Theme::Color::Overlay).a, 1.0F / 255.0F);

    // A dialog that closes keeps fading with its backdrop, and both are gone at the end of the transition.
    gui->set("quit", {{"open", false}});
    frames(2);
    const std::optional<float> closing = getBackdropAlpha();
    ASSERT_TRUE(closing.has_value());
    EXPECT_LT(*closing, getUi().getTheme().getColor(Theme::Color::Overlay).a);
    frames(getTransitionFrames());
    EXPECT_FALSE(getBackdropAlpha().has_value());
}

// Opening a toast again replaces its notice, and only the end of the notice that shows reports that the toast was dismissed.
TEST_F(OverlayTest, DismissesAToastOnceWhenItWasOpenedAgain) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "toast", "id": "saved", "text": "Saved", "duration": 1, "open": true}]})");
    frames(10);
    gui->set("saved", {{"open", true}});
    frames(30);
    EXPECT_EQ(std::ranges::count(getEventNames(), std::string("saved:dismiss")), 0);
    frames(90);
    EXPECT_EQ(std::ranges::count(getEventNames(), std::string("saved:dismiss")), 1);
    EXPECT_EQ(findLastEvent("dismiss").value, (core::Json{{"reason", "timeout"}}));
}

// A notice of a list is as wide as what it says, and the notices of a deck line up at the width of the theme.
TEST_F(OverlayTest, FitsNoticesOfAListToTheirTextAndLinesUpTheNoticesOfADeck) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "children": [
        {"kind": "toast", "id": "short", "text": "Saved", "duration": 0, "open": true},
        {"kind": "toast", "id": "long", "text": "Two new items arrived in the chest by the dock", "duration": 0, "open": true, "position": "bottom"},
        {"kind": "toast", "id": "deck", "text": "Saved", "duration": 0, "open": true, "position": "topEnd", "arrangement": "deck"}
    ]})");
    // clang-format on
    frames(getTransitionFrames());
    const float width = getUi().getTheme().getMetric(Theme::Metric::ToastWidth);
    EXPECT_LT(getBounds(*gui, "short").width, getBounds(*gui, "long").width);
    EXPECT_LT(getBounds(*gui, "long").width, width + 0.5F);
    EXPECT_FLOAT_EQ(getBounds(*gui, "deck").width, width);

    // A single line rounds into a pill.
    EXPECT_GT(getBounds(*gui, "short").width, getBounds(*gui, "short").height);
}

TEST_F(OverlayTest, PutsTheNewestNoticeOfADeckInFrontAndOpensTheDeckUnderThePointer) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "toast", "id": "news", "position": "bottom", "arrangement": "deck", "duration": 0}]})");
    for (const char* message : {"One", "Two", "Three"}) {
        gui->command(getUi().getContext(), "news", "show", {{"text", message}});
    }
    frames(getTransitionFrames() * 3);

    // The newest notice is in front at the edge, and the older ones peek out above it, a little smaller.
    std::vector<math::Rect> closed = drawMessages();
    ASSERT_EQ(closed.size(), 3U);
    std::ranges::sort(closed, [](const math::Rect& first, const math::Rect& second) { return first.y > second.y; });
    const float peek = getUi().getTheme().getMetric(Theme::Metric::ToastPeek);
    EXPECT_LT(closed[0].y - closed[1].y, peek * 1.5F);

    // The pointer over the deck opens it into a list, and the time of its notices stops meanwhile.
    pointer(platform::Event::Type::MouseMove, getBounds(*gui, "news").getCenter());
    frames(getTransitionFrames() * 3);
    std::vector<math::Rect> opened = drawMessages();
    ASSERT_EQ(opened.size(), 3U);
    std::ranges::sort(opened, [](const math::Rect& first, const math::Rect& second) { return first.y > second.y; });
    EXPECT_GT(opened[0].y - opened[1].y, closed[0].height * 2.0F);
    EXPECT_FLOAT_EQ(opened[0].height, closed[0].height);
    EXPECT_LT(closed[1].width, opened[1].width * 0.97F);
    EXPECT_LT(closed[2].width, opened[2].width * 0.93F);
}

// A deck that the pointer opens over a notice behind its front stays open while the pointer is anywhere in it, though the notice moves away from the pointer as the deck opens.
TEST_F(OverlayTest, KeepsADeckOpenWhileThePointerStaysInIt) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "toast", "id": "news", "position": "bottom", "arrangement": "deck", "duration": 0}]})");
    for (const char* message : {"One", "Two", "Three"}) {
        gui->command(getUi().getContext(), "news", "show", {{"text", message}});
    }
    frames(getTransitionFrames() * 3);
    const math::Rect front = getBounds(*gui, "news");
    const float peek = getUi().getTheme().getMetric(Theme::Metric::ToastPeek);
    pointer(platform::Event::Type::MouseMove, {front.getCenter().x, front.y - peek * 0.5F});
    frames(getTransitionFrames() * 3);
    for (int frame = 0; frame < 20; ++frame) {
        std::vector<math::Rect> opened = drawMessages();
        ASSERT_EQ(opened.size(), 3U);
        std::ranges::sort(opened, [](const math::Rect& first, const math::Rect& second) { return first.y > second.y; });
        EXPECT_GT(opened[0].y - opened[2].y, front.height * 2.0F);
    }
}

TEST_F(OverlayTest, StopsTheTimeOfNoticesUnderThePointer) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "toast", "id": "note", "text": "Hold me", "duration": 0.5, "open": true}]})");
    frames(getTransitionFrames());
    pointer(platform::Event::Type::MouseMove, getBounds(*gui, "note").getCenter());
    frames(60);
    EXPECT_EQ(drawMessages().size(), 1U);

    // The time runs on once the pointer leaves.
    pointer(platform::Event::Type::MouseMove, {1.0F, 1.0F});
    frames(60);
    EXPECT_TRUE(drawMessages().empty());
}

TEST_F(OverlayTest, ChangesAndHidesNoticesByTheirIdAndReportsWhyEachLeft) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "toast", "id": "jobs", "duration": 0}]})");
    gui->command(getUi().getContext(), "jobs", "show", {{"notice", "upload"}, {"text", "Uploading"}, {"indicator", "busy"}});
    frames(getTransitionFrames());
    ASSERT_EQ(drawMessages().size(), 1U);
    EXPECT_EQ(findLastEvent("noticeShow").value, (core::Json{{"notice", "upload"}}));

    // An update changes the notice in place, and a duration it gives starts the time of the notice again.
    gui->command(getUi().getContext(), "jobs", "update", {{"notice", "upload"}, {"text", "Uploaded"}, {"tone", "success"}, {"indicator", "icon"}, {"duration", 0.5}});
    frames(20);
    EXPECT_EQ(drawMessages().size(), 1U);
    frames(30);
    EXPECT_TRUE(drawMessages().empty());
    EXPECT_EQ(findLastEvent("dismiss").value, (core::Json{{"reason", "timeout"}, {"notice", "upload"}}));

    gui->command(getUi().getContext(), "jobs", "show", {{"notice", "a"}, {"text", "First"}});
    gui->command(getUi().getContext(), "jobs", "show", {{"notice", "b"}, {"text", "Second"}});
    frames(getTransitionFrames());
    gui->command(getUi().getContext(), "jobs", "hide", {{"notice", "a"}});
    frames(getTransitionFrames());
    EXPECT_EQ(drawMessages().size(), 1U);
    EXPECT_EQ(findLastEvent("dismiss").value, (core::Json{{"reason", "hidden"}, {"notice", "a"}}));
    gui->command(getUi().getContext(), "jobs", "hide", core::Json::object());
    frames(getTransitionFrames());
    EXPECT_TRUE(drawMessages().empty());

    EXPECT_THROW(gui->command(getUi().getContext(), "jobs", "update", {{"text", "Lost"}}), std::invalid_argument);
    EXPECT_THROW(gui->command(getUi().getContext(), "jobs", "update", {{"notice", "missing"}}), std::invalid_argument);
    EXPECT_THROW(gui->command(getUi().getContext(), "jobs", "hide", {{"notice", "missing"}}), std::invalid_argument);
    EXPECT_THROW(gui->command(getUi().getContext(), "jobs", "show", {{"variant", "glass"}}), std::invalid_argument);
    EXPECT_THROW(gui->command(getUi().getContext(), "jobs", "show", {{"progress", 2}}), std::invalid_argument);
}

TEST_F(OverlayTest, ClosesNoticesWithTheirButtonsAndSwipesThemAway) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "toast", "id": "note", "duration": 0, "position": "bottom"}]})");
    const float padding = getUi().getTheme().getMetric(Theme::Metric::ControlPaddingX);

    // A press on the notice reports a click and lets the notice go.
    gui->command(getUi().getContext(), "note", "show", {{"notice", "pressed"}, {"text", "Open the cart"}});
    frames(getTransitionFrames() * 2);
    click(getBounds(*gui, "note").getCenter());
    frames(getTransitionFrames() * 2);
    EXPECT_EQ(findLastEvent("click").value, (core::Json{{"notice", "pressed"}}));
    EXPECT_EQ(findLastEvent("dismiss").value, (core::Json{{"reason", "clicked"}, {"notice", "pressed"}}));
    EXPECT_TRUE(drawMessages().empty());

    // The close button sits at the end of the notice.
    gui->command(getUi().getContext(), "note", "show", {{"notice", "closing"}, {"text", "The file was deleted"}, {"closable", true}});
    frames(getTransitionFrames() * 2);
    math::Rect frame = getBounds(*gui, "note");
    click({frame.getRight() - padding - 6.0F, frame.getCenter().y});
    frames(getTransitionFrames() * 2);
    EXPECT_EQ(findLastEvent("dismiss").value, (core::Json{{"reason", "closed"}, {"notice", "closing"}}));
    EXPECT_TRUE(drawMessages().empty());

    // The action reports itself and lets the notice go.
    gui->command(getUi().getContext(), "note", "show", {{"notice", "undo"}, {"text", "The file was deleted"}, {"action", "Undo"}});
    frames(getTransitionFrames() * 2);
    frame = getBounds(*gui, "note");
    click({frame.getRight() - padding - 6.0F, frame.getCenter().y});
    frames(getTransitionFrames() * 2);
    EXPECT_EQ(findLastEvent("action").value, (core::Json{{"notice", "undo"}}));
    EXPECT_EQ(findLastEvent("dismiss").value, (core::Json{{"reason", "action"}, {"notice", "undo"}}));

    // A short swipe springs back, and a long one sends the notice away.
    gui->command(getUi().getContext(), "note", "show", {{"notice", "swiped"}, {"text", "Swipe me away"}});
    frames(getTransitionFrames() * 2);
    frame = getBounds(*gui, "note");
    clearEvents();
    drag(frame.getCenter(), frame.getCenter() + math::Vec2{frame.width * 0.15F, 0.0F});
    frames(30);
    EXPECT_EQ(std::ranges::count(getEventNames(), std::string("note:click")), 0);
    EXPECT_NEAR(getBounds(*gui, "note").x, frame.x, 1.0F);
    EXPECT_EQ(drawMessages().size(), 1U);
    drag(frame.getCenter(), frame.getCenter() + math::Vec2{frame.width * 0.6F, 0.0F});
    frames(30);
    EXPECT_TRUE(drawMessages().empty());
    EXPECT_EQ(findLastEvent("dismiss").value, (core::Json{{"reason", "swiped"}, {"notice", "swiped"}}));
}

} // namespace haylen::ui
