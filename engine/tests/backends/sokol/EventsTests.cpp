#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include "backends/sokol/Events.hpp"
#include "haylen/platform/Event.hpp"

namespace haylen::sokol {

class EventsTest : public ::testing::Test {
  protected:
    [[nodiscard]] static sapp_event makeEvent(sapp_event_type type) {
        sapp_event event{};
        event.type = type;
        return event;
    }
};

TEST_F(EventsTest, TranslatesKeysTextAndMouse) {
    sapp_event key = makeEvent(SAPP_EVENTTYPE_KEY_DOWN);
    key.key_code = SAPP_KEYCODE_SPACE;
    key.key_repeat = true;
    key.modifiers = SAPP_MODIFIER_SHIFT | SAPP_MODIFIER_CTRL | SAPP_MODIFIER_ALT | SAPP_MODIFIER_SUPER;
    const platform::Event down = *Events::translate(key);
    EXPECT_EQ(down.type, platform::Event::Type::KeyDown);
    EXPECT_EQ(down.key, input::Key::Space);
    EXPECT_TRUE(down.repeat);
    EXPECT_EQ(down.modifiers, (input::KeyModifiers{.shift = true, .control = true, .alt = true, .super = true}));

    key.type = SAPP_EVENTTYPE_KEY_UP;
    key.key_code = SAPP_KEYCODE_ESCAPE;
    key.modifiers = 0;
    EXPECT_EQ(Events::translate(key)->key, input::Key::Escape);
    EXPECT_EQ(Events::translate(key)->modifiers, input::KeyModifiers{});

    sapp_event character = makeEvent(SAPP_EVENTTYPE_CHAR);
    character.char_code = 0x00E9;
    EXPECT_EQ(Events::translate(character)->character, U'é');

    sapp_event mouse = makeEvent(SAPP_EVENTTYPE_MOUSE_DOWN);
    mouse.mouse_button = SAPP_MOUSEBUTTON_RIGHT;
    mouse.mouse_x = 10.0F;
    mouse.mouse_y = 20.0F;
    const platform::Event pressed = *Events::translate(mouse);
    EXPECT_EQ(pressed.mouseButton, input::MouseButton::Right);
    EXPECT_EQ(pressed.position, math::Vec2(10.0F, 20.0F));
    mouse.type = SAPP_EVENTTYPE_MOUSE_UP;
    mouse.mouse_button = SAPP_MOUSEBUTTON_MIDDLE;
    EXPECT_EQ(Events::translate(mouse)->mouseButton, input::MouseButton::Middle);
    mouse.mouse_button = SAPP_MOUSEBUTTON_LEFT;
    EXPECT_EQ(Events::translate(mouse)->type, platform::Event::Type::MouseUp);

    sapp_event move = makeEvent(SAPP_EVENTTYPE_MOUSE_MOVE);
    move.mouse_dx = 3.0F;
    move.mouse_dy = -2.0F;
    EXPECT_EQ(Events::translate(move)->delta, math::Vec2(3.0F, -2.0F));

    sapp_event scroll = makeEvent(SAPP_EVENTTYPE_MOUSE_SCROLL);
    scroll.scroll_y = 1.5F;
    EXPECT_EQ(Events::translate(scroll)->scroll, math::Vec2(0.0F, 1.5F));
}

TEST_F(EventsTest, TranslatesTouchesAndLifecycle) {
    sapp_event touches = makeEvent(SAPP_EVENTTYPE_TOUCHES_BEGAN);
    touches.num_touches = 2;
    touches.touches[0] = {.identifier = 11, .pos_x = 1.0F, .pos_y = 2.0F, .changed = true};
    touches.touches[1] = {.identifier = 12, .pos_x = 3.0F, .pos_y = 4.0F, .changed = false};
    const platform::Event began = *Events::translate(touches);
    ASSERT_EQ(began.touchCount, 2U);
    EXPECT_EQ(began.touches[1].id, 12U);
    EXPECT_EQ(began.touches[0].position, math::Vec2(1.0F, 2.0F));
    EXPECT_TRUE(began.touches[0].changed);

    const std::vector<std::pair<sapp_event_type, platform::Event::Type>> mapping{
        {SAPP_EVENTTYPE_TOUCHES_MOVED, platform::Event::Type::TouchMoved}, {SAPP_EVENTTYPE_TOUCHES_ENDED, platform::Event::Type::TouchEnded}, {SAPP_EVENTTYPE_TOUCHES_CANCELLED, platform::Event::Type::TouchCancelled}, {SAPP_EVENTTYPE_MOUSE_ENTER, platform::Event::Type::MouseEnter}, {SAPP_EVENTTYPE_MOUSE_LEAVE, platform::Event::Type::MouseLeave}, {SAPP_EVENTTYPE_RESIZED, platform::Event::Type::Resized}, {SAPP_EVENTTYPE_ICONIFIED, platform::Event::Type::Suspended}, {SAPP_EVENTTYPE_SUSPENDED, platform::Event::Type::Suspended}, {SAPP_EVENTTYPE_RESTORED, platform::Event::Type::Resumed}, {SAPP_EVENTTYPE_RESUMED, platform::Event::Type::Resumed}, {SAPP_EVENTTYPE_FOCUSED, platform::Event::Type::FocusGained}, {SAPP_EVENTTYPE_UNFOCUSED, platform::Event::Type::FocusLost}, {SAPP_EVENTTYPE_QUIT_REQUESTED, platform::Event::Type::QuitRequested},
    };
    for (const auto& [source, expected] : mapping) {
        EXPECT_EQ(Events::translate(makeEvent(source))->type, expected) << static_cast<int>(source);
    }
    EXPECT_FALSE(Events::translate(makeEvent(SAPP_EVENTTYPE_CLIPBOARD_PASTED)).has_value());
    EXPECT_FALSE(Events::translate(makeEvent(SAPP_EVENTTYPE_FILES_DROPPED)).has_value());
}

} // namespace haylen::sokol
