#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <imgui_internal.h>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/Shape.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/math/Geometry.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/text/Direction.hpp"
#include "haylen/ui/Backend.hpp"
#include "haylen/ui/Message.hpp"
#include "haylen/ui/Navigation.hpp"
#include "haylen/ui/Theme.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/AllocationTracker.hpp"
#include "support/LuaUiFixture.hpp"

namespace haylen::ui {

// The messages show over an empty screen, and screen readers read the tree, where each message is an alert whose frame the tests find by what it says.
class MessageTest : public ::testing::Test, public test::LuaUiFixture {
  protected:
    MessageTest() {
        getFixture().host().setAccessibilityTreeWanted(true);
        frames(2);
    }

    std::shared_ptr<Message> show(const std::string& properties) {
        return getUi().getNavigation().showMessage(core::Json::parse(properties));
    }

    // The frames of the messages that drew in the last frame, in the points that the pointer takes, the one that says the label or all of them.
    [[nodiscard]] std::optional<math::Rect> findMessage(std::string_view label) {
        for (const platform::AccessibilityElement& element : getAlerts()) {
            if (element.label == label) {
                return toPoints(element.bounds);
            }
        }

        return std::nullopt;
    }

    // The frame of the message that says the label, which the test needs to have drawn in the last frame.
    [[nodiscard]] math::Rect requireMessage(std::string_view label) {
        const std::optional<math::Rect> found = findMessage(label);
        if (!found) {
            throw std::runtime_error("The message \"" + std::string(label) + "\" did not draw in the last frame.");
        }

        return *found;
    }

    [[nodiscard]] std::vector<math::Rect> getMessageFrames() {
        std::vector<math::Rect> found;
        for (const platform::AccessibilityElement& element : getAlerts()) {
            found.push_back(toPoints(element.bounds));
        }

        // clang-format off
        std::ranges::sort(found, [](const math::Rect& first, const math::Rect& second) {
            return first.y > second.y;
        });
        // clang-format on
        return found;
    }

    // Runs a frame and returns the blocks of text the renderer drew, the texts of the messages in these tests, from the bottom up.
    [[nodiscard]] std::vector<math::Rect> drawTexts() {
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
        std::ranges::sort(blocks, [](const math::Rect& first, const math::Rect& second) { return first.y > second.y; });
        return blocks;
    }

    // Runs frames for a number of seconds.
    void wait(float seconds) {
        frames(static_cast<int>(seconds * 60.0F));
    }

  private:
    [[nodiscard]] std::vector<platform::AccessibilityElement> getAlerts() {
        std::vector<platform::AccessibilityElement> alerts;
        const std::vector<platform::HeadlessHost::AccessibilityTree>& trees = getFixture().host().getAccessibilityTrees();
        if (trees.empty()) {
            return alerts;
        }

        for (const platform::AccessibilityElement& element : trees.back().elements) {
            if (element.role == platform::AccessibilityElement::Role::Alert) {
                alerts.push_back(element);
            }
        }

        return alerts;
    }

    [[nodiscard]] math::Rect toPoints(const math::Rect& framebuffer) {
        const graphics::Viewport& viewport = getEngine().getViewport();
        const math::Vec2 origin = viewport.getVisibleRect().getMin();
        return math::Rect::fromMinMax(viewport.toDesign(framebuffer.getMin()) - origin, viewport.toDesign(framebuffer.getMax()) - origin);
    }
};

// A toast is a pill at the bottom middle that fits its text, a snackbar fills the width at the bottom, and a banner fills the width at the top of the safe area, under the status bar.
TEST_F(MessageTest, GivesEachStyleItsPlaceAndShape) {
    getFixture().host().setSafeAreaInsets({.top = 60.0F});
    static_cast<void>(show(R"({"text": "Saved", "duration": 0})"));
    static_cast<void>(show(R"({"style": "snackbar", "text": "Photo deleted", "action": "Undo", "duration": 0})"));
    static_cast<void>(show(R"({"style": "banner", "title": "Ana", "text": "See you at six", "duration": 0})"));
    frames(getTransitionFrames() * 3);

    const math::Rect safe = getUi().getBackend().getLiftedSafeRect();
    const float margin = getMetric(Theme::Metric::PanelPadding);
    const float width = std::min(getMetric(Theme::Metric::ToastWidth), safe.width - margin * 2.0F);
    const std::optional<math::Rect> toast = findMessage("Saved");
    const std::optional<math::Rect> snackbar = findMessage("Photo deleted. Undo");
    const std::optional<math::Rect> banner = findMessage("Ana. See you at six");
    ASSERT_TRUE(toast && snackbar && banner);

    EXPECT_NEAR(toast->getCenter().x, safe.getCenter().x, 1.0F);
    EXPECT_GT(toast->width, toast->height);
    EXPECT_LT(toast->width, width * 0.5F);
    EXPECT_NEAR(toast->getBottom(), safe.getBottom() - margin, 1.0F);
    EXPECT_NEAR(snackbar->width, width, 1.0F);
    EXPECT_LT(snackbar->getBottom(), toast->y);
    EXPECT_NEAR(banner->width, width, 1.0F);
    EXPECT_NEAR(banner->y, safe.y + margin, 1.0F);
    EXPECT_GE(safe.y, 60.0F);
}

// A toast lasts 3 seconds, a snackbar 4, or 6 with an action the person needs time to reach, and a banner 5, from the frame it comes on screen.
TEST_F(MessageTest, StaysForTheDurationOfItsStyle) {
    const std::shared_ptr<Message> toast = show(R"({"text": "Saved"})");
    const std::shared_ptr<Message> snackbar = show(R"({"style": "snackbar", "text": "Archived"})");
    const std::shared_ptr<Message> banner = show(R"({"style": "banner", "text": "New message"})");
    frames(2);
    EXPECT_TRUE(toast->isShown() && snackbar->isShown() && banner->isShown());

    wait(2.9F);
    EXPECT_FALSE(toast->isDismissed());
    wait(0.2F);
    EXPECT_EQ(toast->getReason(), Message::Reason::Timeout);
    wait(0.8F);
    EXPECT_FALSE(snackbar->isDismissed());
    wait(0.2F);
    EXPECT_TRUE(snackbar->isDismissed());
    wait(0.8F);
    EXPECT_FALSE(banner->isDismissed());
    wait(0.2F);
    EXPECT_TRUE(banner->isDismissed());

    const std::shared_ptr<Message> undo = show(R"({"style": "snackbar", "text": "Deleted", "action": "Undo"})");
    frames(getTransitionFrames());
    EXPECT_TRUE(undo->isShown());
    wait(5.6F);
    EXPECT_FALSE(undo->isDismissed());
    wait(0.6F);
    EXPECT_TRUE(undo->isDismissed());
}

TEST_F(MessageTest, StacksToastsThatShowTogetherWithoutCoveringThem) {
    const std::shared_ptr<Message> first = show(R"({"text": "The first message", "position": "top", "duration": 0})");
    frames();
    static_cast<void>(show(R"({"text": "The second message", "position": "top", "duration": 0})"));
    static_cast<void>(show(R"({"text": "At the bottom", "position": "bottomEnd", "duration": 0})"));
    frames(getTransitionFrames() * 3);

    const math::Rect top = requireMessage("The first message");
    const math::Rect second = requireMessage("The second message");
    const math::Rect bottom = requireMessage("At the bottom");
    EXPECT_GE(second.y, top.getBottom());
    EXPECT_NEAR(second.getCenter().x, top.getCenter().x, 1.0F);
    EXPECT_GT(bottom.y, second.getBottom());
    EXPECT_GT(bottom.x, top.x);

    // Once the first message leaves, the second one moves up into its place.
    first->dismiss();
    frames(getTransitionFrames() * 3);
    EXPECT_EQ(requireMessage("The second message").y, top.y);
    EXPECT_FALSE(findMessage("The first message").has_value());
    EXPECT_EQ(first->getReason(), Message::Reason::Dismissed);
}

TEST_F(MessageTest, QueuesToastsBeyondTheLimitOfTheStack) {
    std::vector<std::shared_ptr<Message>> shown;
    for (const char* text : {"One", "Two", "Three", "Four"}) {
        shown.push_back(getUi().getNavigation().showMessage({{"text", text}, {"duration", 0.5}}));
    }

    frames(getTransitionFrames());
    EXPECT_EQ(getMessageFrames().size(), 3U);
    EXPECT_FALSE(shown[3]->isShown());

    // The fourth message waits until the first ones leave, and its own time starts once it shows.
    frames(40);
    EXPECT_EQ(getMessageFrames().size(), 1U);
    EXPECT_TRUE(shown[3]->isShown());
    frames(40);
    EXPECT_TRUE(getMessageFrames().empty());

    EXPECT_THROW(static_cast<void>(show(R"({"text": "Hi", "color": "red"})")), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(show(R"({"text": "Hi", "duration": -1})")), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(show(R"({"text": "Hi", "style": "dialog"})")), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(show(R"({"duration": 1})")), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(show(R"("Hi")")), std::invalid_argument);
}

// Snackbars show one at a time and the others wait, a higher priority before a lower one, while a toast never makes a snackbar wait.
TEST_F(MessageTest, ShowsOneSnackbarAtATimeInTheOrderOfItsPriority) {
    const std::shared_ptr<Message> toast = show(R"({"text": "Saved", "duration": 0})");
    const std::shared_ptr<Message> first = show(R"({"style": "snackbar", "text": "First", "duration": 0})");
    const std::shared_ptr<Message> low = show(R"({"style": "snackbar", "text": "Low", "priority": "low", "duration": 0})");
    const std::shared_ptr<Message> normal = show(R"({"style": "snackbar", "text": "Normal", "duration": 0})");
    frames(getTransitionFrames() * 2);
    EXPECT_TRUE(toast->isShown());
    EXPECT_TRUE(first->isShown());
    EXPECT_FALSE(low->isShown() || normal->isShown());

    first->dismiss();
    frames(getTransitionFrames() * 2);
    EXPECT_TRUE(normal->isShown());
    EXPECT_FALSE(low->isShown());

    normal->dismiss();
    frames(getTransitionFrames() * 2);
    EXPECT_TRUE(low->isShown());
    EXPECT_EQ(getMessageFrames().size(), 2U);
}

// A snackbar of a higher priority lets the one that shows go with the reason "replaced" and comes on screen once it left, while a banner of the same priority waits its turn.
TEST_F(MessageTest, ReplacesTheMessageThatShowsWithOneOfAHigherPriority) {
    const std::shared_ptr<Message> saved = show(R"({"style": "snackbar", "text": "Draft saved", "duration": 0})");
    const std::shared_ptr<Message> news = show(R"({"style": "banner", "text": "Ana wrote", "duration": 0})");
    frames(getTransitionFrames());
    const std::shared_ptr<Message> offline = show(R"({"style": "snackbar", "text": "You are offline", "priority": "high", "duration": 0})");
    const std::shared_ptr<Message> later = show(R"({"style": "banner", "text": "Ben wrote", "duration": 0})");
    frames();
    EXPECT_EQ(saved->getReason(), Message::Reason::Replaced);
    EXPECT_FALSE(offline->isShown());
    frames(getTransitionFrames() * 2);
    EXPECT_TRUE(offline->isShown());
    EXPECT_FALSE(later->isShown());
    EXPECT_FALSE(news->isDismissed());
    EXPECT_FALSE(findMessage("Draft saved").has_value());
}

// A toast is as wide as what it says, and the messages of a deck line up at the width of the theme.
TEST_F(MessageTest, FitsToastsToTheirTextAndLinesUpTheMessagesOfADeck) {
    static_cast<void>(show(R"({"text": "Saved", "position": "top", "duration": 0})"));
    static_cast<void>(show(R"({"text": "Two new items arrived in the chest by the dock", "duration": 0})"));
    static_cast<void>(show(R"({"text": "Stored", "position": "topEnd", "arrangement": "deck", "duration": 0})"));
    frames(getTransitionFrames());
    const float width = getMetric(Theme::Metric::ToastWidth);
    EXPECT_LT(requireMessage("Saved").width, requireMessage("Two new items arrived in the chest by the dock").width);
    EXPECT_LT(requireMessage("Two new items arrived in the chest by the dock").width, width + 0.5F);
    EXPECT_FLOAT_EQ(requireMessage("Stored").width, width);
}

// The indicator of a pill nests in its round end, as far from the start as from the top and the bottom.
TEST_F(MessageTest, NestsTheIndicatorOfAToastInTheRoundEndOfItsPill) {
    static_cast<void>(show(R"({"text": "Your profile is up to date.", "indicator": "icon", "duration": 0})"));
    frames(getTransitionFrames());

    // A neutral message fills its frame with the tooltip color and its indicator with the color of the text on it, and the frame is the largest shape of its color.
    const Theme& theme = getUi().getTheme();
    std::optional<math::Rect> frame;
    std::optional<math::Rect> indicator;
    for (const graphics2d::Shape& shape : getUi().getBackend().getShapes()) {
        const float area = shape.bounds.width * shape.bounds.height;
        if (shape.color == theme.getColor(Theme::Color::Tooltip) && (!frame || area > frame->width * frame->height)) {
            frame = shape.bounds;
        }

        if (shape.color == theme.getColor(Theme::Color::OnTooltip) && (!indicator || area > indicator->width * indicator->height)) {
            indicator = shape.bounds;
        }
    }

    ASSERT_TRUE(frame.has_value());
    ASSERT_TRUE(indicator.has_value());
    const float top = indicator->y - frame->y;
    EXPECT_GT(top, 0.0F);
    EXPECT_NEAR(frame->getBottom() - indicator->getBottom(), top, 1.0F);
    EXPECT_NEAR(indicator->x - frame->x, top, 1.0F);
}

TEST_F(MessageTest, PutsTheNewestMessageOfADeckInFrontAndOpensTheDeckUnderThePointer) {
    for (const char* text : {"One", "Two", "Three"}) {
        static_cast<void>(getUi().getNavigation().showMessage({{"text", text}, {"arrangement", "deck"}, {"duration", 0}}));
    }

    frames(getTransitionFrames() * 3);

    // The newest message is in front at the edge, and the older ones peek out above it, a little smaller, which only their drawn text shows.
    const std::vector<math::Rect> closed = drawTexts();
    ASSERT_EQ(closed.size(), 3U);
    const float peek = getMetric(Theme::Metric::ToastPeek);
    EXPECT_LT(closed[0].y - closed[1].y, peek * 1.5F);

    // The pointer over the deck opens it into a list, and the time of its messages stops meanwhile.
    pointer(platform::Event::Type::MouseMove, requireMessage("Three").getCenter());
    frames(getTransitionFrames() * 3);
    const std::vector<math::Rect> opened = drawTexts();
    ASSERT_EQ(opened.size(), 3U);
    EXPECT_GT(opened[0].y - opened[1].y, closed[0].height * 2.0F);
    EXPECT_FLOAT_EQ(opened[0].height, closed[0].height);
    EXPECT_LT(closed[1].width, opened[1].width * 0.97F);
    EXPECT_LT(closed[2].width, opened[2].width * 0.93F);
}

// A deck that the pointer opens over a message behind its front stays open while the pointer is anywhere in it, though the message moves away from the pointer as the deck opens.
TEST_F(MessageTest, KeepsADeckOpenWhileThePointerStaysInIt) {
    for (const char* text : {"One", "Two", "Three"}) {
        static_cast<void>(getUi().getNavigation().showMessage({{"text", text}, {"arrangement", "deck"}, {"duration", 0}}));
    }

    frames(getTransitionFrames() * 3);
    const math::Rect front = requireMessage("Three");
    const float peek = getMetric(Theme::Metric::ToastPeek);
    pointer(platform::Event::Type::MouseMove, {front.getCenter().x, front.y - peek * 0.5F});
    frames(getTransitionFrames() * 3);
    const std::vector<math::Rect> opened = getMessageFrames();
    ASSERT_EQ(opened.size(), 3U);

    // The deck stays open in every frame with the pointer on the message behind, in the gaps between the messages and just past the front one.
    const float gap = (opened[1].getBottom() + opened[0].y) * 0.5F;
    for (const float y : {front.y - peek * 0.5F, gap, front.getBottom() + 4.0F}) {
        pointer(platform::Event::Type::MouseMove, {front.getCenter().x, y});

        for (int frame = 0; frame < 20; ++frame) {
            frames();
            const std::vector<math::Rect> shown = getMessageFrames();
            ASSERT_EQ(shown.size(), 3U);
            EXPECT_GT(shown[0].y - shown[2].y, front.height * 2.0F);
        }
    }
}

// A deck that never opens keeps its depth under the pointer, and each tap takes the front message away while the next one comes forward.
TEST_F(MessageTest, TakesTheFrontMessageOfADeckThatNeverOpensAwayWithEachTap) {
    std::vector<std::shared_ptr<Message>> deck;
    for (const char* text : {"One", "Two", "Three"}) {
        deck.push_back(getUi().getNavigation().showMessage({{"text", text}, {"arrangement", "deck"}, {"expandable", false}, {"duration", 0}}));
    }

    frames(getTransitionFrames() * 3);
    const math::Rect front = requireMessage("Three");
    const float peek = getMetric(Theme::Metric::ToastPeek);
    pointer(platform::Event::Type::MouseMove, front.getCenter());
    frames(getTransitionFrames() * 3);
    const std::vector<math::Rect> closed = getMessageFrames();
    ASSERT_EQ(closed.size(), 3U);
    EXPECT_LT(closed[0].y - closed[2].y, peek * 3.0F);

    for (std::size_t left = 2; left > 0; --left) {
        click(front.getCenter());
        frames(getTransitionFrames() * 3);
        const std::vector<math::Rect> shown = getMessageFrames();
        ASSERT_EQ(shown.size(), left);
        EXPECT_NEAR(shown[0].getBottom(), closed[0].getBottom(), 1.0F);
    }

    EXPECT_EQ(deck[2]->getReason(), Message::Reason::Tapped);
    EXPECT_EQ(deck[1]->getReason(), Message::Reason::Tapped);
}

TEST_F(MessageTest, StopsTheTimeOfMessagesUnderThePointer) {
    const std::shared_ptr<Message> held = show(R"({"text": "Hold me", "duration": 0.5})");
    frames(getTransitionFrames());
    pointer(platform::Event::Type::MouseMove, requireMessage("Hold me").getCenter());
    frames(60);
    EXPECT_FALSE(held->isDismissed());

    // The time runs on once the pointer leaves.
    pointer(platform::Event::Type::MouseMove, {1.0F, 1.0F});
    frames(60);
    EXPECT_TRUE(getMessageFrames().empty());
}

// An update changes a message in place and a duration it gives starts its time again, a dismissal lets it go, and the signals tell each step once.
TEST_F(MessageTest, UpdatesAndDismissesMessagesAndTellsWhyEachLeft) {
    const std::shared_ptr<Message> upload = show(R"({"title": "Upload", "text": "Sending the save", "indicator": "progress", "progress": 0.25, "duration": 0})");
    std::vector<std::string> heard;
    static_cast<void>(upload->appeared.connect([&heard](Message&) { heard.emplace_back("appeared"); }));
    // clang-format off
    static_cast<void>(upload->dismissed.connect([&heard](Message& left) {
        ASSERT_TRUE(left.getReason().has_value());
        heard.emplace_back(Message::reasonName(*left.getReason()));
    }));
    // clang-format on
    frames(getTransitionFrames());
    ASSERT_TRUE(findMessage("Upload. Sending the save").has_value());

    upload->update({{"text", "Sent"}, {"tone", "success"}, {"indicator", "icon"}, {"duration", 0.5}});
    frames(20);
    EXPECT_TRUE(findMessage("Upload. Sent").has_value());
    frames(30);
    EXPECT_TRUE(getMessageFrames().empty());
    EXPECT_EQ(heard, (std::vector<std::string>{"appeared", "timeout"}));

    // A message dismissed while it waits leaves the queue at once and never comes on screen.
    const std::shared_ptr<Message> queued = show(R"({"style": "banner", "text": "First", "duration": 0})");
    const std::shared_ptr<Message> waiting = show(R"({"style": "banner", "text": "Second", "duration": 0})");
    frames(getTransitionFrames());
    waiting->dismiss();
    queued->dismiss();
    frames(getTransitionFrames());
    EXPECT_FALSE(waiting->isShown());
    EXPECT_TRUE(getMessageFrames().empty());
    EXPECT_TRUE(getUi().getNavigation().getMessages().empty());

    EXPECT_THROW(upload->update({{"style", "banner"}}), std::invalid_argument);
    EXPECT_THROW(upload->update({{"variant", "glass"}}), std::invalid_argument);
    EXPECT_THROW(upload->update({{"progress", 2}}), std::invalid_argument);
    EXPECT_THROW(upload->update({{"title", ""}, {"text", ""}}), std::invalid_argument);
}

// Each message takes the window of a message that left, so ImGui keeps only as many windows as messages showed at once.
TEST_F(MessageTest, ReusesTheWindowsOfMessagesThatLeft) {
    for (int index = 0; index < 12; ++index) {
        const std::shared_ptr<Message> message = show(R"({"text": "Saved", "duration": 0})");
        frames(getTransitionFrames());
        message->dismiss();
        frames(getTransitionFrames());
    }

    getUi().getBackend().makeCurrent();
    const std::ptrdiff_t windows = std::ranges::count_if(GImGui->Windows, [](const ImGuiWindow* window) { return std::string_view(window->Name).starts_with("##haylen-message"); });
    EXPECT_EQ(windows, 1);
}

TEST_F(MessageTest, ClosesMessagesWithTheirButtonsAndSwipesThemAway) {
    const float padding = getMetric(Theme::Metric::ControlPaddingX);
    std::vector<std::string> heard;

    // A tap on the message lets it go.
    const std::shared_ptr<Message> tapped = show(R"({"text": "Open the cart", "duration": 0})");
    static_cast<void>(tapped->tapped.connect([&heard](Message&) { heard.emplace_back("tapped"); }));
    frames(getTransitionFrames() * 2);
    click(requireMessage("Open the cart").getCenter());
    frames(getTransitionFrames() * 2);
    EXPECT_EQ(tapped->getReason(), Message::Reason::Tapped);
    EXPECT_TRUE(getMessageFrames().empty());

    // The close button sits at the end of the message.
    const std::shared_ptr<Message> closed = show(R"({"text": "The file was deleted", "closable": true, "duration": 0})");
    frames(getTransitionFrames() * 2);
    math::Rect frame = requireMessage("The file was deleted");
    click({frame.getRight() - padding - 6.0F, frame.getCenter().y});
    frames(getTransitionFrames() * 2);
    EXPECT_EQ(closed->getReason(), Message::Reason::Closed);

    // The action tells the app and lets the message go.
    const std::shared_ptr<Message> undo = show(R"({"style": "snackbar", "text": "The file was deleted", "action": "Undo", "duration": 0})");
    static_cast<void>(undo->actionPressed.connect([&heard](Message&) { heard.emplace_back("action"); }));
    frames(getTransitionFrames() * 2);
    frame = requireMessage("The file was deleted. Undo");
    click({frame.getRight() - padding - 6.0F, frame.getCenter().y});
    frames(getTransitionFrames() * 2);
    EXPECT_EQ(undo->getReason(), Message::Reason::Action);
    EXPECT_EQ(heard, (std::vector<std::string>{"tapped", "action"}));

    // A short swipe springs back, and a long one sends the message away.
    const std::shared_ptr<Message> swiped = show(R"({"text": "Swipe me away", "duration": 0})");
    frames(getTransitionFrames() * 2);
    frame = requireMessage("Swipe me away");
    drag(frame.getCenter(), frame.getCenter() + math::Vec2{frame.width * 0.15F, 0.0F});
    frames(30);
    EXPECT_FALSE(swiped->isDismissed());
    EXPECT_NEAR(requireMessage("Swipe me away").x, frame.x, 1.0F);
    drag(frame.getCenter(), frame.getCenter() + math::Vec2{frame.width * 0.6F, 0.0F});
    frames(30);
    EXPECT_TRUE(getMessageFrames().empty());
    EXPECT_EQ(swiped->getReason(), Message::Reason::Swiped);
}

// Screen readers hear every message that comes on screen once, and a high priority interrupts what they say.
TEST_F(MessageTest, AnnouncesEveryMessageThatComesOnScreen) {
    static_cast<void>(show(R"({"text": "Saved", "duration": 0})"));
    static_cast<void>(show(R"({"style": "banner", "title": "Ana", "text": "See you at six", "priority": "high", "duration": 0})"));
    frames(getTransitionFrames());

    const std::vector<platform::HeadlessHost::Announcement>& announcements = getFixture().host().getAnnouncements();
    ASSERT_EQ(announcements.size(), 2U);
    EXPECT_EQ(announcements[0].text, "Saved");
    EXPECT_FALSE(announcements[0].interrupt);
    EXPECT_EQ(announcements[1].text, "Ana. See you at six");
    EXPECT_TRUE(announcements[1].interrupt);
}

// A message at the start of an edge lines up with the right side of a right to left interface, and it fades in where it rests while the system asks for reduced motion.
TEST_F(MessageTest, FollowsTheDirectionAndReducedMotion) {
    getUi().setDirection(text::Direction::RightToLeft);
    getFixture().host().setAccessibilitySettings({.reducedMotion = true});
    frames(2);
    static_cast<void>(show(R"({"text": "Saved", "position": "topStart", "duration": 0})"));
    frames(2);
    const math::Rect arriving = requireMessage("Saved");
    frames(getTransitionFrames());
    const math::Rect rested = requireMessage("Saved");
    const math::Rect safe = getUi().getBackend().getLiftedSafeRect();
    EXPECT_NEAR(rested.getRight(), safe.getRight() - getMetric(Theme::Metric::PanelPadding), 1.0F);
    EXPECT_EQ(arriving, rested);
}

// Messages that stay where they are cost the frames nothing more than the frame alone.
TEST_F(MessageTest, ShowsStillMessagesWithoutAllocating) {
    // clang-format off
    const auto measure = [this] {
        frames(5);
        const test::AllocationTracker tracker;
        frames(10);
        return tracker.getTotal();
    };
    // clang-format on
    const std::size_t frameAlone = measure();
    static_cast<void>(show(R"({"text": "Saved", "indicator": "icon", "duration": 0})"));
    static_cast<void>(show(R"({"style": "snackbar", "text": "Photo deleted", "action": "Undo", "closable": true, "duration": 0})"));
    static_cast<void>(show(R"({"style": "banner", "title": "Ana", "text": "See you at six", "indicator": "busy", "duration": 0})"));
    frames(getTransitionFrames() * 3);
    EXPECT_EQ(measure(), frameAlone);
}

// A message from Lua tells its handlers what happened, resolves its result with the reason it left, and leaves at once when its owner ends.
TEST_F(MessageTest, ShowsMessagesFromLua) {
    // clang-format off
    getFixture().runLua(R"(
        local async = require('async')
        ui = require('haylen.ui')
        heard = {}
        undo = ui.showMessage{style = 'snackbar', text = 'Photo deleted', action = 'Undo', duration = 0,
            onShow = function(event) heard[#heard + 1] = 'show:' .. tostring(event.message == undo) end,
            onAction = function(event) heard[#heard + 1] = 'action' end,
            onDismiss = function(event) heard[#heard + 1] = 'dismiss:' .. event.reason end}
        async.spawn(function() reason = undo.result:await() end)
        upload = ui.showMessage{text = 'Sending', indicator = 'progress', progress = 0.25, duration = 0, position = 'top', onTap = function() heard[#heard + 1] = 'tap' end}
    )");
    // clang-format on
    frames(getTransitionFrames());
    EXPECT_EQ(getFixture().lua("return undo.style .. ' ' .. tostring(undo.shown) .. ' ' .. tostring(undo.dismissed) .. ' ' .. tostring(undo.result == undo.result)"), "snackbar true false true");

    const float padding = getMetric(Theme::Metric::ControlPaddingX);
    const math::Rect frame = requireMessage("Photo deleted. Undo");
    click({frame.getRight() - padding - 6.0F, frame.getCenter().y});
    frames(getTransitionFrames());
    EXPECT_EQ(getFixture().lua("return table.concat(heard, ' ') .. ' ' .. tostring(reason)"), "show:true action dismiss:action action");

    getFixture().runLua("upload:update({text = 'Sent', tone = 'success', indicator = 'icon'})");
    frames(2);
    EXPECT_TRUE(findMessage("Sent").has_value());
    click(requireMessage("Sent").getCenter());
    frames(getTransitionFrames());
    EXPECT_EQ(getFixture().lua("return heard[#heard]"), "tap");

    // A message whose owner ends leaves at once without telling the handlers of that owner, and its result still says why.
    // clang-format off
    getFixture().runLua(R"(
        local async = require('async')
        hud = ui.mount(ui.label{text = 'Hud'})
        news = ui.showMessage{style = 'banner', title = 'Ana', text = 'See you at six', duration = 0, owner = hud, onDismiss = function() heard[#heard + 1] = 'owned' end}
        async.spawn(function() left = news.result:await() end)
    )");
    // clang-format on
    frames(getTransitionFrames());
    EXPECT_TRUE(findMessage("Ana. See you at six").has_value());
    getFixture().runLua("hud:unmount()");
    frames(3);
    EXPECT_FALSE(findMessage("Ana. See you at six").has_value());
    EXPECT_EQ(getFixture().lua("return left .. ' ' .. tostring(news.dismissed) .. ' ' .. heard[#heard]"), "dismissed true tap");

    // A message dismissed before it came on screen resolves its result as well.
    getFixture().runLua("late = ui.showMessage{text = 'Later', duration = 0} late:dismiss() require('async').spawn(function() lateReason = late.result:await() end)");
    frames(2);
    EXPECT_EQ(getFixture().lua("return lateReason .. ' ' .. tostring(late.shown)"), "dismissed false");
}

// The messages keep inside the area of the app, above its bars, while no modal presentation covers it, and lie in the safe area over a presentation and once the GUI of the area unmounts.
TEST_F(MessageTest, KeepsTheMessagesInsideTheAreaOfTheApp) {
    // clang-format off
    getFixture().runLua(R"(
        ui = require('haylen.ui')
        frame = ui.mount(ui.column{gap = 0, ui.column{id = 'middle', grow = 1}, ui.row{id = 'bar', height = 160}})
        ui.setMessageArea{gui = frame, id = 'middle'}
        ui.showMessage{style = 'snackbar', text = 'Photo deleted', duration = 0}
    )");
    // clang-format on
    frames(getTransitionFrames() * 3);
    const math::Rect safe = getUi().getBackend().getLiftedSafeRect();
    const float margin = getMetric(Theme::Metric::PanelPadding);
    ASSERT_TRUE(findMessage("Photo deleted"));
    EXPECT_NEAR(requireMessage("Photo deleted").getBottom(), safe.getBottom() - 160.0F - margin, 1.0F);

    getFixture().runLua("sheet = ui.present({build = function() return ui.column{ui.label{text = 'Queue'}} end})");
    frames(40);
    EXPECT_NEAR(requireMessage("Photo deleted").getBottom(), safe.getBottom() - margin, 1.0F);

    getFixture().runLua("sheet:dismiss()");
    frames(40);
    EXPECT_NEAR(requireMessage("Photo deleted").getBottom(), safe.getBottom() - 160.0F - margin, 1.0F);

    getFixture().runLua("frame:unmount()");
    frames(2);
    EXPECT_FALSE(getUi().getNavigation().getMessageArea().has_value());
    EXPECT_NEAR(requireMessage("Photo deleted").getBottom(), safe.getBottom() - margin, 1.0F);

    getFixture().runLua("other = ui.mount(ui.column{ui.label{text = 'Other'}})");
    EXPECT_NE(getFixture().lua("ui.setMessageArea{gui = other, id = 'nowhere'}").find("The GUI of the region has no node with the id \"nowhere\"."), std::string::npos);
    EXPECT_NE(getFixture().lua("ui.setMessageArea{gui = other, node = 'middle'}").find("Unknown option \"node\"."), std::string::npos);
    EXPECT_EQ(getFixture().lua("ui.setMessageArea(nil)").find("error:"), std::string::npos);
}

TEST_F(MessageTest, ReportsMistakesFromLua) {
    getFixture().runLua("ui = require('haylen.ui') shown = ui.showMessage{text = 'Hi'}");
    EXPECT_NE(getFixture().lua("ui.showMessage{text = 'Hi', color = 'red'}").find("Unknown option \"color\"."), std::string::npos);
    EXPECT_NE(getFixture().lua("ui.showMessage{text = 'Hi', style = 'dialog'}").find("The property \"style\" of a \"message\" must be \"toast\", \"snackbar\" or \"banner\"."), std::string::npos);
    EXPECT_NE(getFixture().lua("ui.showMessage{text = 'Hi', priority = 'urgent'}").find("must be \"low\", \"normal\" or \"high\""), std::string::npos);
    EXPECT_NE(getFixture().lua("ui.showMessage{duration = 2}").find("A message needs a \"title\" or a \"text\"."), std::string::npos);
    EXPECT_NE(getFixture().lua("ui.showMessage{text = 'Hi', onShow = 5}").find("function expected"), std::string::npos);
    EXPECT_NE(getFixture().lua("shown:update({style = 'banner'})").find("Unknown option \"style\"."), std::string::npos);
    EXPECT_NE(getFixture().lua("shown:update({progress = 3})").find("The property \"progress\" of a \"message\" must be from 0 to 1."), std::string::npos);
}

} // namespace haylen::ui
