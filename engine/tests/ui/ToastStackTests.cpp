#include <gtest/gtest.h>

#include <cstdint>
#include <optional>

#include "haylen/ui/ToastStack.hpp"

namespace haylen::ui {

TEST(ToastStackTest, StacksNoticesInTheOrderTheyAskedToShow) {
    ToastStack stack;
    const std::uint64_t first = stack.createKey();
    const std::uint64_t second = stack.createKey();
    const std::uint64_t below = stack.createKey();

    // A notice takes its place after the notices before it, from the last frame or placed already in this one, so notices that place themselves out of order settle in the frame after.
    stack.beginFrame();
    EXPECT_EQ(stack.place(second, ToastStack::Position::Top, 80.0F, 2.0, 3), 0.0F);
    EXPECT_EQ(stack.place(first, ToastStack::Position::Top, 100.0F, 1.0, 3), 0.0F);
    EXPECT_EQ(stack.place(below, ToastStack::Position::Bottom, 60.0F, 0.5, 3), 0.0F);

    stack.beginFrame();
    EXPECT_EQ(stack.place(second, ToastStack::Position::Top, 80.0F, 2.0, 3), 100.0F);
    EXPECT_EQ(stack.place(first, ToastStack::Position::Top, 100.0F, 1.0, 3), 0.0F);
    EXPECT_EQ(stack.place(below, ToastStack::Position::Bottom, 60.0F, 0.5, 3), 0.0F);

    // A notice that leaves gives its place back as its length shrinks.
    stack.beginFrame();
    EXPECT_EQ(stack.place(first, ToastStack::Position::Top, 40.0F, 1.0, 3), 0.0F);
    EXPECT_EQ(stack.place(second, ToastStack::Position::Top, 80.0F, 2.0, 3), 100.0F);
    stack.beginFrame();
    EXPECT_EQ(stack.place(second, ToastStack::Position::Top, 80.0F, 2.0, 3), 40.0F);
}

TEST(ToastStackTest, MakesNoticesWaitWhileTheStackIsFull) {
    ToastStack stack;
    const std::uint64_t keys[] = {stack.createKey(), stack.createKey(), stack.createKey()};
    for (int frame = 0; frame < 2; ++frame) {
        stack.beginFrame();
        EXPECT_EQ(stack.place(keys[0], ToastStack::Position::Bottom, 50.0F, 1.0, 2), 0.0F);
        EXPECT_EQ(stack.place(keys[1], ToastStack::Position::Bottom, 50.0F, 2.0, 2), 50.0F);
        EXPECT_FALSE(stack.place(keys[2], ToastStack::Position::Bottom, 50.0F, 3.0, 2).has_value());
    }

    // Once the first notice is gone, the waiting one takes the room it left in the frame after.
    stack.beginFrame();
    (void)stack.place(keys[1], ToastStack::Position::Bottom, 50.0F, 2.0, 2);
    EXPECT_FALSE(stack.place(keys[2], ToastStack::Position::Bottom, 50.0F, 3.0, 2).has_value());
    stack.beginFrame();
    EXPECT_EQ(stack.place(keys[1], ToastStack::Position::Bottom, 50.0F, 2.0, 2), 0.0F);
    EXPECT_EQ(stack.place(keys[2], ToastStack::Position::Bottom, 50.0F, 3.0, 2), 50.0F);
}

TEST(ToastStackTest, ShowsEveryNoticeWithALimitOfZero) {
    ToastStack stack;
    const std::uint64_t keys[] = {stack.createKey(), stack.createKey(), stack.createKey()};
    std::optional<float> last;
    for (int frame = 0; frame < 2; ++frame) {
        stack.beginFrame();
        for (int index = 0; index < 3; ++index) {
            last = stack.place(keys[index], ToastStack::Position::Top, 30.0F, index, 0);
        }
    }
    EXPECT_EQ(last, 60.0F);
}

} // namespace haylen::ui
