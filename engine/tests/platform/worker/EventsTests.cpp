#include <gtest/gtest.h>

#include <optional>

#include "platform/worker/Events.hpp"

namespace haylen::platform::worker {

TEST(WorkerEventsTest, TranslatesKeysWithTheirModifiers) {
    const std::optional<Event> down = Events::translateKey("KeyW", true, true, 1 | 8);
    ASSERT_TRUE(down);
    EXPECT_EQ(down->type, Event::Type::KeyDown);
    EXPECT_EQ(down->key, input::Key::W);
    EXPECT_TRUE(down->repeat);
    EXPECT_EQ(down->modifiers, (input::KeyModifiers{.shift = true, .super = true}));

    const std::optional<Event> up = Events::translateKey("Escape", false, false, 2 | 4);
    ASSERT_TRUE(up);
    EXPECT_EQ(up->type, Event::Type::KeyUp);
    EXPECT_EQ(up->modifiers, (input::KeyModifiers{.control = true, .alt = true}));

    EXPECT_FALSE(Events::translateKey("AudioVolumeUp", true, false, 0));
}

TEST(WorkerEventsTest, PlacesTheMouseInFramebufferPixels) {
    const std::optional<Event> move = Events::translateMouse(Events::Mouse::Move, 0, {10.0F, 20.0F}, {1.0F, -2.0F}, 0, 2.0F);
    ASSERT_TRUE(move);
    EXPECT_EQ(move->type, Event::Type::MouseMove);
    EXPECT_EQ(move->position, (math::Vec2{20.0F, 40.0F}));
    EXPECT_EQ(move->delta, (math::Vec2{2.0F, -4.0F}));

    const std::optional<Event> press = Events::translateMouse(Events::Mouse::Down, 2, {5.0F, 5.0F}, {}, 2, 1.5F);
    ASSERT_TRUE(press);
    EXPECT_EQ(press->type, Event::Type::MouseDown);
    EXPECT_EQ(press->mouseButton, input::MouseButton::Right);
    EXPECT_EQ(press->position, (math::Vec2{7.5F, 7.5F}));
    EXPECT_TRUE(press->modifiers.control);

    const std::optional<Event> middle = Events::translateMouse(Events::Mouse::Up, 1, {}, {}, 0, 1.0F);
    ASSERT_TRUE(middle);
    EXPECT_EQ(middle->mouseButton, input::MouseButton::Middle);
}

TEST(WorkerEventsTest, LeavesTheButtonsBeyondTheThirdOut) {
    EXPECT_FALSE(Events::translateMouse(Events::Mouse::Down, 3, {}, {}, 0, 1.0F));
    EXPECT_FALSE(Events::translateMouse(Events::Mouse::Up, 4, {}, {}, 0, 1.0F));
}

TEST(WorkerEventsTest, KeepsTheUnitsOfAScroll) {
    const Event precise = Events::translateScroll({0.0F, -12.0F}, true, 4);
    EXPECT_EQ(precise.type, Event::Type::MouseScroll);
    EXPECT_EQ(precise.scroll, (math::Vec2{0.0F, -12.0F}));
    EXPECT_TRUE(precise.preciseScroll);
    EXPECT_TRUE(precise.modifiers.alt);
    EXPECT_FALSE(Events::translateScroll({1.0F, 0.0F}, false, 0).preciseScroll);
}

TEST(WorkerEventsTest, HandsEveryTouchWithEachChange) {
    Events events;
    const std::optional<Event> first = events.translateTouch(7, Touches::Phase::Began, {10.0F, 10.0F}, 0, 2.0F);
    ASSERT_TRUE(first);
    EXPECT_EQ(first->type, Event::Type::TouchBegan);
    ASSERT_EQ(first->touchCount, 1U);
    EXPECT_EQ(first->touches[0].position, (math::Vec2{20.0F, 20.0F}));

    const std::optional<Event> second = events.translateTouch(9, Touches::Phase::Began, {30.0F, 5.0F}, 0, 2.0F);
    ASSERT_TRUE(second);
    ASSERT_EQ(second->touchCount, 2U);
    EXPECT_FALSE(second->touches[0].changed);
    EXPECT_TRUE(second->touches[1].changed);

    const std::optional<Event> lifted = events.translateTouch(7, Touches::Phase::Ended, {12.0F, 10.0F}, 0, 2.0F);
    ASSERT_TRUE(lifted);
    EXPECT_EQ(lifted->type, Event::Type::TouchEnded);
    EXPECT_EQ(lifted->touchCount, 2U);

    const std::optional<Event> after = events.translateTouch(9, Touches::Phase::Moved, {31.0F, 5.0F}, 0, 2.0F);
    ASSERT_TRUE(after);
    ASSERT_EQ(after->touchCount, 1U);
    EXPECT_EQ(after->touches[0].id, 9U);
}

TEST(WorkerEventsTest, IgnoresAFingerThatIsNotDown) {
    Events events;
    EXPECT_FALSE(events.translateTouch(3, Touches::Phase::Moved, {}, 0, 1.0F));
    EXPECT_FALSE(events.translateTouch(3, Touches::Phase::Ended, {}, 0, 1.0F));
}

} // namespace haylen::platform::worker
