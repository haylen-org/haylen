#include <gtest/gtest.h>

#include <algorithm>
#include <string_view>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/ui/Gui.hpp"
#include "haylen/ui/Scaling.hpp"
#include "haylen/ui/Theme.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/UiFixture.hpp"

namespace haylen::ui {

namespace {

class OverlayTest : public ::testing::Test, public test::UiFixture {
  protected:
    // The bounds of the element with the label in the last accessibility tree the platform received.
    [[nodiscard]] math::Rect findElement(std::string_view label) {
        const std::vector<platform::AccessibilityElement>& elements = getFixture().host().getAccessibilityTrees().back().elements;
        const auto found = std::ranges::find(elements, label, &platform::AccessibilityElement::label);
        EXPECT_NE(found, elements.end()) << label;
        return found == elements.end() ? math::Rect{} : found->bounds;
    }

    [[nodiscard]] int getTransitionFrames() {
        return static_cast<int>(getUi().getTheme().getMetric(Theme::Metric::TransitionDuration) * 60.0F) + 2;
    }
};

} // namespace

// Answers that fit line up in one row at the end of the dialog, and answers that do not fit stack across its width with the main answer on top, so none passes the edges of the dialog.
TEST_F(OverlayTest, StacksTheAnswersOfADialogThatDoNotFitInOneRow) {
    getFixture().host().setAccessibilityTreeWanted(true);
    getUi().setScaling({.mode = Scaling::Mode::Physical, .factor = 1.0F});
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "dialog", "id": "safety", "title": "Safety", "buttons": [{"id": "close", "text": "Close"}, {"id": "share", "text": "Share my trip"}, {"id": "help", "text": "Call for help", "variant": "destructive"}]}]})");
    frames(getTransitionFrames());
    EXPECT_EQ(findElement("Close").y, findElement("Call for help").y);
    EXPECT_LT(findElement("Close").getRight(), findElement("Share my trip").x);

    // A phone in portrait leaves the dialog too narrow for the row.
    getFixture().host().resize({480.0F, 1000.0F});
    frames(3);
    const math::Rect help = findElement("Call for help");
    const math::Rect share = findElement("Share my trip");
    const math::Rect close = findElement("Close");
    EXPECT_LT(help.getBottom(), share.y);
    EXPECT_LT(share.getBottom(), close.y);

    for (const math::Rect& button : {help, share, close}) {
        EXPECT_EQ(button.x, help.x);
        EXPECT_EQ(button.width, help.width);
        EXPECT_GT(button.x, 0.0F);
        EXPECT_LT(button.getRight(), 480.0F);
        EXPECT_LT(button.getBottom(), 1000.0F);
    }
}

} // namespace haylen::ui
