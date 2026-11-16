#include <gtest/gtest.h>

#include <cstdint>
#include <optional>

#include "haylen/ui/Message.hpp"
#include "haylen/ui/MessageStack.hpp"

namespace haylen::ui {

namespace {

class MessageStackTest : public ::testing::Test {
  protected:
    static constexpr float kPeek = 10.0F;

    // Places a message of a list that shows already and returns its distance from the edge, or nothing while it waits.
    std::optional<float> place(std::uint64_t key, MessageStack::Position position, float extent, double since, std::size_t limit, bool shown = true, Message::Priority priority = Message::Priority::Normal) {
        const std::optional<MessageStack::Placement> placement = stack.place({.key = key, .position = position, .extent = extent, .since = since, .priority = priority, .shown = shown, .limit = limit, .peek = kPeek});
        return placement ? std::optional<float>(placement->offset) : std::nullopt;
    }

    MessageStack::Placement placeInDeck(std::uint64_t key, float extent, double since, bool expandable = true) {
        return *stack.place({.key = key, .position = MessageStack::Position::Bottom, .arrangement = MessageStack::Arrangement::Deck, .expandable = expandable, .extent = extent, .since = since, .shown = true, .limit = 2, .peek = kPeek});
    }

    MessageStack stack;
};

} // namespace

TEST_F(MessageStackTest, StacksMessagesInTheOrderTheyCameOnScreen) {
    const std::uint64_t first = stack.createKey();
    const std::uint64_t second = stack.createKey();
    const std::uint64_t below = stack.createKey();

    // A message takes its place after the messages before it, from the last frame or placed already in this one, so messages that place themselves out of order settle in the frame after.
    stack.beginFrame();
    EXPECT_EQ(place(second, MessageStack::Position::Top, 80.0F, 2.0, 3), 0.0F);
    EXPECT_EQ(place(first, MessageStack::Position::Top, 100.0F, 1.0, 3), 0.0F);
    EXPECT_EQ(place(below, MessageStack::Position::Bottom, 60.0F, 0.5, 3), 0.0F);

    stack.beginFrame();
    EXPECT_EQ(place(second, MessageStack::Position::Top, 80.0F, 2.0, 3), 100.0F);
    EXPECT_EQ(place(first, MessageStack::Position::Top, 100.0F, 1.0, 3), 0.0F);
    EXPECT_EQ(place(below, MessageStack::Position::Bottom, 60.0F, 0.5, 3), 0.0F);

    // A message that leaves gives its place back as its length shrinks.
    stack.beginFrame();
    EXPECT_EQ(place(first, MessageStack::Position::Top, 40.0F, 1.0, 3), 0.0F);
    EXPECT_EQ(place(second, MessageStack::Position::Top, 80.0F, 2.0, 3), 100.0F);
    stack.beginFrame();
    EXPECT_EQ(place(second, MessageStack::Position::Top, 80.0F, 2.0, 3), 40.0F);
}

TEST_F(MessageStackTest, MakesMessagesWaitWhileTheStackIsFull) {
    const std::uint64_t keys[] = {stack.createKey(), stack.createKey(), stack.createKey()};
    for (int frame = 0; frame < 2; ++frame) {
        stack.beginFrame();
        EXPECT_EQ(place(keys[0], MessageStack::Position::Bottom, 50.0F, 1.0, 2), 0.0F);
        EXPECT_EQ(place(keys[1], MessageStack::Position::Bottom, 50.0F, 2.0, 2), 50.0F);
        EXPECT_FALSE(place(keys[2], MessageStack::Position::Bottom, 50.0F, 3.0, 2, false).has_value());
    }

    // Once the first message is gone, the waiting one takes the room it left in the frame after.
    stack.beginFrame();
    (void)place(keys[1], MessageStack::Position::Bottom, 50.0F, 2.0, 2);
    EXPECT_FALSE(place(keys[2], MessageStack::Position::Bottom, 50.0F, 3.0, 2, false).has_value());
    stack.beginFrame();
    EXPECT_EQ(place(keys[1], MessageStack::Position::Bottom, 50.0F, 2.0, 2), 0.0F);
    EXPECT_EQ(place(keys[2], MessageStack::Position::Bottom, 50.0F, 3.0, 2, false), 50.0F);
}

// The messages that wait take their turn by priority and then in the order they asked, and a message that shows keeps its place before them, whatever their priority.
TEST_F(MessageStackTest, LetsAHigherPriorityWaitAheadOfALowerOne) {
    const std::uint64_t shown = stack.createKey();
    const std::uint64_t low = stack.createKey();
    const std::uint64_t normal = stack.createKey();
    const std::uint64_t high = stack.createKey();
    for (int frame = 0; frame < 2; ++frame) {
        stack.beginFrame();
        EXPECT_EQ(place(shown, MessageStack::Position::Bottom, 50.0F, 5.0, 1, true, Message::Priority::Low), 0.0F);
        EXPECT_FALSE(place(low, MessageStack::Position::Bottom, 50.0F, 1.0, 1, false, Message::Priority::Low).has_value());
        EXPECT_FALSE(place(normal, MessageStack::Position::Bottom, 50.0F, 2.0, 1, false).has_value());
        EXPECT_FALSE(place(high, MessageStack::Position::Bottom, 50.0F, 3.0, 1, false, Message::Priority::High).has_value());
    }

    // Each message that leaves gives its room to the next one in the frame after the one it left in.
    for (int frame = 0; frame < 2; ++frame) {
        stack.beginFrame();
        const bool turn = frame == 1;
        EXPECT_FALSE(place(low, MessageStack::Position::Bottom, 50.0F, 1.0, 1, false, Message::Priority::Low).has_value());
        EXPECT_FALSE(place(normal, MessageStack::Position::Bottom, 50.0F, 2.0, 1, false).has_value());
        EXPECT_EQ(place(high, MessageStack::Position::Bottom, 50.0F, 3.0, 1, false, Message::Priority::High).has_value(), turn);
    }

    stack.beginFrame();
    EXPECT_EQ(place(high, MessageStack::Position::Bottom, 50.0F, 6.0, 1, true, Message::Priority::High), 0.0F);
    EXPECT_FALSE(place(low, MessageStack::Position::Bottom, 50.0F, 1.0, 1, false, Message::Priority::Low).has_value());
    EXPECT_FALSE(place(normal, MessageStack::Position::Bottom, 50.0F, 2.0, 1, false).has_value());

    for (int frame = 0; frame < 2; ++frame) {
        stack.beginFrame();
        const bool turn = frame == 1;
        EXPECT_FALSE(place(low, MessageStack::Position::Bottom, 50.0F, 1.0, 1, false, Message::Priority::Low).has_value());
        EXPECT_EQ(place(normal, MessageStack::Position::Bottom, 50.0F, 2.0, 1, false).has_value(), turn);
    }
}

TEST_F(MessageStackTest, ShowsEveryMessageWithALimitOfZero) {
    const std::uint64_t keys[] = {stack.createKey(), stack.createKey(), stack.createKey()};
    std::optional<float> last;
    for (int frame = 0; frame < 2; ++frame) {
        stack.beginFrame();

        for (int index = 0; index < 3; ++index) {
            last = place(keys[index], MessageStack::Position::Top, 30.0F, index, 0);
        }
    }

    EXPECT_EQ(last, 60.0F);
}

TEST_F(MessageStackTest, PutsTheNewestMessageOfADeckInFrontAndOpensTheDeckWhileItIsHeld) {
    const std::uint64_t keys[] = {stack.createKey(), stack.createKey(), stack.createKey()};
    MessageStack::Placement placements[3];

    for (int frame = 0; frame < 2; ++frame) {
        stack.beginFrame();

        for (int index = 0; index < 3; ++index) {
            placements[index] = placeInDeck(keys[index], 50.0F + static_cast<float>(index) * 10.0F, index);
        }
    }

    // The newest message is in front at the edge, each older one lies a peek further in, and a deck never makes a message wait.
    EXPECT_EQ(placements[2].depth, 0U);
    EXPECT_EQ(placements[2].offset, 0.0F);
    EXPECT_EQ(placements[1].depth, 1U);
    EXPECT_EQ(placements[1].offset, kPeek);
    EXPECT_EQ(placements[0].depth, 2U);
    EXPECT_EQ(placements[0].offset, kPeek * 2.0F);

    // A deck held in a frame opens into a list from the newest message in the next one.
    stack.hold(MessageStack::Position::Bottom);
    EXPECT_FALSE(stack.isHeld(MessageStack::Position::Bottom));
    stack.beginFrame();
    EXPECT_TRUE(stack.isHeld(MessageStack::Position::Bottom));
    EXPECT_FALSE(stack.isHeld(MessageStack::Position::Top));

    for (int index = 0; index < 3; ++index) {
        placements[index] = placeInDeck(keys[index], 50.0F + static_cast<float>(index) * 10.0F, index);
    }

    EXPECT_EQ(placements[2].offset, 0.0F);
    EXPECT_EQ(placements[1].offset, 70.0F);
    EXPECT_EQ(placements[0].offset, 130.0F);
    EXPECT_EQ(placements[0].depth, 0U);

    // A deck that is not expandable keeps its depth while its stack is held.
    stack.hold(MessageStack::Position::Bottom);
    stack.beginFrame();
    EXPECT_EQ(placeInDeck(keys[0], 50.0F, 0, false).depth, 2U);

    stack.beginFrame();
    EXPECT_FALSE(stack.isHeld(MessageStack::Position::Bottom));
}

} // namespace haylen::ui
