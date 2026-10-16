#include <gtest/gtest.h>

#include <cstdint>
#include <optional>

#include "haylen/ui/ToastStack.hpp"

namespace haylen::ui {

namespace {

class ToastStackTest : public ::testing::Test {
  protected:
    static constexpr float kPeek = 10.0F;

    // Places a notice of a list and returns its distance from the edge, or nothing while it waits.
    std::optional<float> place(std::uint64_t key, ToastStack::Position position, float extent, double since, std::size_t limit) {
        const std::optional<ToastStack::Placement> placement = stack.place(key, position, ToastStack::Arrangement::List, extent, since, limit, kPeek);
        return placement ? std::optional<float>(placement->offset) : std::nullopt;
    }

    ToastStack::Placement placeInDeck(std::uint64_t key, float extent, double since, std::size_t limit) {
        return *stack.place(key, ToastStack::Position::Bottom, ToastStack::Arrangement::Deck, extent, since, limit, kPeek);
    }

    ToastStack stack;
};

} // namespace

TEST_F(ToastStackTest, StacksNoticesInTheOrderTheyAskedToShow) {
    const std::uint64_t first = stack.createKey();
    const std::uint64_t second = stack.createKey();
    const std::uint64_t below = stack.createKey();

    // A notice takes its place after the notices before it, from the last frame or placed already in this one, so notices that place themselves out of order settle in the frame after.
    stack.beginFrame();
    EXPECT_EQ(place(second, ToastStack::Position::Top, 80.0F, 2.0, 3), 0.0F);
    EXPECT_EQ(place(first, ToastStack::Position::Top, 100.0F, 1.0, 3), 0.0F);
    EXPECT_EQ(place(below, ToastStack::Position::Bottom, 60.0F, 0.5, 3), 0.0F);

    stack.beginFrame();
    EXPECT_EQ(place(second, ToastStack::Position::Top, 80.0F, 2.0, 3), 100.0F);
    EXPECT_EQ(place(first, ToastStack::Position::Top, 100.0F, 1.0, 3), 0.0F);
    EXPECT_EQ(place(below, ToastStack::Position::Bottom, 60.0F, 0.5, 3), 0.0F);

    // A notice that leaves gives its place back as its length shrinks.
    stack.beginFrame();
    EXPECT_EQ(place(first, ToastStack::Position::Top, 40.0F, 1.0, 3), 0.0F);
    EXPECT_EQ(place(second, ToastStack::Position::Top, 80.0F, 2.0, 3), 100.0F);
    stack.beginFrame();
    EXPECT_EQ(place(second, ToastStack::Position::Top, 80.0F, 2.0, 3), 40.0F);
}

TEST_F(ToastStackTest, MakesNoticesWaitWhileTheStackIsFull) {
    const std::uint64_t keys[] = {stack.createKey(), stack.createKey(), stack.createKey()};
    for (int frame = 0; frame < 2; ++frame) {
        stack.beginFrame();
        EXPECT_EQ(place(keys[0], ToastStack::Position::Bottom, 50.0F, 1.0, 2), 0.0F);
        EXPECT_EQ(place(keys[1], ToastStack::Position::Bottom, 50.0F, 2.0, 2), 50.0F);
        EXPECT_FALSE(place(keys[2], ToastStack::Position::Bottom, 50.0F, 3.0, 2).has_value());
    }

    // Once the first notice is gone, the waiting one takes the room it left in the frame after.
    stack.beginFrame();
    (void)place(keys[1], ToastStack::Position::Bottom, 50.0F, 2.0, 2);
    EXPECT_FALSE(place(keys[2], ToastStack::Position::Bottom, 50.0F, 3.0, 2).has_value());
    stack.beginFrame();
    EXPECT_EQ(place(keys[1], ToastStack::Position::Bottom, 50.0F, 2.0, 2), 0.0F);
    EXPECT_EQ(place(keys[2], ToastStack::Position::Bottom, 50.0F, 3.0, 2), 50.0F);
}

TEST_F(ToastStackTest, ShowsEveryNoticeWithALimitOfZero) {
    const std::uint64_t keys[] = {stack.createKey(), stack.createKey(), stack.createKey()};
    std::optional<float> last;
    for (int frame = 0; frame < 2; ++frame) {
        stack.beginFrame();
        for (int index = 0; index < 3; ++index) {
            last = place(keys[index], ToastStack::Position::Top, 30.0F, index, 0);
        }
    }
    EXPECT_EQ(last, 60.0F);
}

TEST_F(ToastStackTest, PutsTheNewestNoticeOfADeckInFrontAndOpensTheDeckWhileItIsHeld) {
    const std::uint64_t keys[] = {stack.createKey(), stack.createKey(), stack.createKey()};
    ToastStack::Placement placements[3];
    for (int frame = 0; frame < 2; ++frame) {
        stack.beginFrame();
        for (int index = 0; index < 3; ++index) {
            placements[index] = placeInDeck(keys[index], 50.0F + static_cast<float>(index) * 10.0F, index, 2);
        }
    }

    // The newest notice is in front at the edge, each older one lies a peek further in, and a deck never makes a notice wait.
    EXPECT_EQ(placements[2].depth, 0U);
    EXPECT_EQ(placements[2].offset, 0.0F);
    EXPECT_EQ(placements[1].depth, 1U);
    EXPECT_EQ(placements[1].offset, kPeek);
    EXPECT_EQ(placements[0].depth, 2U);
    EXPECT_EQ(placements[0].offset, kPeek * 2.0F);

    // A deck held in a frame opens into a list from the newest notice in the next one.
    stack.hold(ToastStack::Position::Bottom);
    EXPECT_FALSE(stack.isHeld(ToastStack::Position::Bottom));
    stack.beginFrame();
    EXPECT_TRUE(stack.isHeld(ToastStack::Position::Bottom));
    EXPECT_FALSE(stack.isHeld(ToastStack::Position::Top));
    for (int index = 0; index < 3; ++index) {
        placements[index] = placeInDeck(keys[index], 50.0F + static_cast<float>(index) * 10.0F, index, 2);
    }
    EXPECT_EQ(placements[2].offset, 0.0F);
    EXPECT_EQ(placements[1].offset, 70.0F);
    EXPECT_EQ(placements[0].offset, 130.0F);
    EXPECT_EQ(placements[0].depth, 0U);

    stack.beginFrame();
    EXPECT_FALSE(stack.isHeld(ToastStack::Position::Bottom));
}

} // namespace haylen::ui
