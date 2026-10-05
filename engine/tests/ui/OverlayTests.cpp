#include <gtest/gtest.h>

#include <cstdint>
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
    EXPECT_EQ(second.x, first.x);
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

} // namespace haylen::ui
