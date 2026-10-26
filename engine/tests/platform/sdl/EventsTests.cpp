#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "haylen/platform/Event.hpp"
#include "platform/sdl/Events.hpp"

namespace haylen::platform::sdl {

class EventsTest : public ::testing::Test {
  protected:
    [[nodiscard]] static SDL_Event makeEvent(SDL_EventType type) {
        SDL_Event event{};
        event.type = type;
        return event;
    }

    [[nodiscard]] static SDL_Event makeFinger(SDL_EventType type, SDL_FingerID id, float x, float y) {
        SDL_Event event = makeEvent(type);
        event.tfinger.fingerID = id;
        event.tfinger.x = x;
        event.tfinger.y = y;
        return event;
    }

    [[nodiscard]] std::vector<platform::Event> translate(const SDL_Event& source, Events::Context context = {}) {
        std::vector<platform::Event> translated;
        events.translate(source, context, translated);
        return translated;
    }

    Events events;
};

TEST_F(EventsTest, TranslatesKeysByTheirPlaceOnTheKeyboard) {
    SDL_Event key = makeEvent(SDL_EVENT_KEY_DOWN);
    key.key.scancode = SDL_SCANCODE_A;
    key.key.down = true;
    key.key.repeat = true;
    key.key.mod = SDL_KMOD_LSHIFT | SDL_KMOD_RCTRL | SDL_KMOD_LALT | SDL_KMOD_RGUI;
    const std::vector<platform::Event> down = translate(key);
    ASSERT_EQ(down.size(), 1U);
    EXPECT_EQ(down[0].type, platform::Event::Type::KeyDown);
    EXPECT_EQ(down[0].key, input::Key::A);
    EXPECT_TRUE(down[0].repeat);
    EXPECT_EQ(down[0].modifiers, (input::KeyModifiers{.shift = true, .control = true, .alt = true, .super = true}));

    key.type = SDL_EVENT_KEY_UP;
    key.key.down = false;
    key.key.scancode = SDL_SCANCODE_KP_ENTER;
    key.key.mod = SDL_KMOD_NONE;
    const std::vector<platform::Event> up = translate(key);
    ASSERT_EQ(up.size(), 1U);
    EXPECT_EQ(up[0].type, platform::Event::Type::KeyUp);
    EXPECT_EQ(up[0].key, input::Key::KeypadEnter);
    EXPECT_EQ(up[0].modifiers, input::KeyModifiers{});

    key.key.scancode = SDL_SCANCODE_MEDIA_PLAY;
    EXPECT_TRUE(translate(key).empty());
}

TEST_F(EventsTest, TurnsTextIntoOneCharacterPerCodePoint) {
    char text[] = "a\xC3\xA9\xF0\x9F\x98\x80";
    SDL_Event input = makeEvent(SDL_EVENT_TEXT_INPUT);
    input.text.text = text;
    const std::vector<platform::Event> characters = translate(input, {.modifiers = SDL_KMOD_SHIFT});
    ASSERT_EQ(characters.size(), 3U);
    EXPECT_EQ(characters[0].character, U'a');
    EXPECT_EQ(characters[1].character, U'é');
    EXPECT_EQ(characters[2].character, U'\U0001F600');
    EXPECT_TRUE(characters[2].modifiers.shift);
}

TEST_F(EventsTest, PlacesTheMouseInFramebufferPixels) {
    SDL_Event button = makeEvent(SDL_EVENT_MOUSE_BUTTON_DOWN);
    button.button.button = SDL_BUTTON_RIGHT;
    button.button.x = 10.0F;
    button.button.y = 20.0F;
    const std::vector<platform::Event> pressed = translate(button, {.density = 2.0F, .modifiers = SDL_KMOD_ALT});
    ASSERT_EQ(pressed.size(), 1U);
    EXPECT_EQ(pressed[0].type, platform::Event::Type::MouseDown);
    EXPECT_EQ(pressed[0].mouseButton, input::MouseButton::Right);
    EXPECT_EQ(pressed[0].position, math::Vec2(20.0F, 40.0F));
    EXPECT_TRUE(pressed[0].modifiers.alt);

    button.type = SDL_EVENT_MOUSE_BUTTON_UP;
    button.button.button = SDL_BUTTON_MIDDLE;
    EXPECT_EQ(translate(button)[0].mouseButton, input::MouseButton::Middle);
    button.button.button = SDL_BUTTON_X1;
    EXPECT_TRUE(translate(button).empty());

    SDL_Event motion = makeEvent(SDL_EVENT_MOUSE_MOTION);
    motion.motion.x = 5.0F;
    motion.motion.y = 6.0F;
    motion.motion.xrel = 3.0F;
    motion.motion.yrel = -2.0F;
    const std::vector<platform::Event> moved = translate(motion, {.density = 2.0F});
    EXPECT_EQ(moved[0].position, math::Vec2(10.0F, 12.0F));
    EXPECT_EQ(moved[0].delta, math::Vec2(6.0F, -4.0F));
}

TEST_F(EventsTest, ScrollsWheelsLeftAndUpWithPositiveSteps) {
    SDL_Event wheel = makeEvent(SDL_EVENT_MOUSE_WHEEL);
    wheel.wheel.x = 1.0F;
    wheel.wheel.y = 2.0F;
    const std::vector<platform::Event> scrolled = translate(wheel);
    ASSERT_EQ(scrolled.size(), 1U);
    EXPECT_EQ(scrolled[0].scroll, math::Vec2(-1.0F, 2.0F));
    EXPECT_FALSE(scrolled[0].preciseScroll);
    EXPECT_EQ(scrolled[0].scrollPhase, platform::Event::ScrollPhase::None);
}

TEST_F(EventsTest, HandsEveryFingerWithEachChange) {
    const Events::Context context{.framebuffer = {200.0F, 100.0F}};
    const std::vector<platform::Event> first = translate(makeFinger(SDL_EVENT_FINGER_DOWN, 7, 0.5F, 0.5F), context);
    ASSERT_EQ(first.size(), 1U);
    EXPECT_EQ(first[0].type, platform::Event::Type::TouchBegan);
    ASSERT_EQ(first[0].touchCount, 1U);
    EXPECT_EQ(first[0].touches[0].position, math::Vec2(100.0F, 50.0F));

    const std::vector<platform::Event> second = translate(makeFinger(SDL_EVENT_FINGER_DOWN, 9, 0.25F, 1.0F), context);
    ASSERT_EQ(second[0].touchCount, 2U);
    EXPECT_FALSE(second[0].touches[0].changed);
    EXPECT_TRUE(second[0].touches[1].changed);
    EXPECT_EQ(second[0].touches[1].id, 9U);

    const std::vector<platform::Event> moved = translate(makeFinger(SDL_EVENT_FINGER_MOTION, 7, 0.0F, 0.0F), context);
    EXPECT_EQ(moved[0].type, platform::Event::Type::TouchMoved);
    EXPECT_TRUE(moved[0].touches[0].changed);
    EXPECT_EQ(moved[0].touches[0].position, math::Vec2(0.0F, 0.0F));

    const std::vector<platform::Event> lifted = translate(makeFinger(SDL_EVENT_FINGER_UP, 7, 0.0F, 0.0F), context);
    EXPECT_EQ(lifted[0].type, platform::Event::Type::TouchEnded);
    EXPECT_EQ(lifted[0].touchCount, 2U);

    const std::vector<platform::Event> cancelled = translate(makeFinger(SDL_EVENT_FINGER_CANCELED, 9, 0.0F, 0.0F), context);
    EXPECT_EQ(cancelled[0].type, platform::Event::Type::TouchCancelled);
    ASSERT_EQ(cancelled[0].touchCount, 1U);
    EXPECT_EQ(cancelled[0].touches[0].id, 9U);

    EXPECT_TRUE(translate(makeFinger(SDL_EVENT_FINGER_MOTION, 9, 0.0F, 0.0F), context).empty());
}

TEST_F(EventsTest, ResumesOnlyAWindowThatWasMinimized) {
    EXPECT_TRUE(translate(makeEvent(SDL_EVENT_WINDOW_RESTORED)).empty());
    EXPECT_EQ(translate(makeEvent(SDL_EVENT_WINDOW_MINIMIZED))[0].type, platform::Event::Type::Suspended);
    EXPECT_EQ(translate(makeEvent(SDL_EVENT_WINDOW_RESTORED))[0].type, platform::Event::Type::Resumed);
    EXPECT_TRUE(translate(makeEvent(SDL_EVENT_WINDOW_RESTORED)).empty());
}

TEST_F(EventsTest, TranslatesTheWindowAndTheApp) {
    const std::vector<std::pair<SDL_EventType, platform::Event::Type>> mapping{
        {SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED, platform::Event::Type::Resized}, {SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED, platform::Event::Type::Resized}, {SDL_EVENT_WINDOW_METAL_VIEW_RESIZED, platform::Event::Type::Resized}, {SDL_EVENT_WINDOW_FOCUS_GAINED, platform::Event::Type::FocusGained}, {SDL_EVENT_WINDOW_FOCUS_LOST, platform::Event::Type::FocusLost}, {SDL_EVENT_WINDOW_MOUSE_ENTER, platform::Event::Type::MouseEnter}, {SDL_EVENT_WINDOW_MOUSE_LEAVE, platform::Event::Type::MouseLeave}, {SDL_EVENT_WINDOW_CLOSE_REQUESTED, platform::Event::Type::QuitRequested}, {SDL_EVENT_QUIT, platform::Event::Type::QuitRequested}, {SDL_EVENT_WILL_ENTER_BACKGROUND, platform::Event::Type::FocusLost}, {SDL_EVENT_DID_ENTER_BACKGROUND, platform::Event::Type::Suspended}, {SDL_EVENT_WILL_ENTER_FOREGROUND, platform::Event::Type::Resumed}, {SDL_EVENT_DID_ENTER_FOREGROUND, platform::Event::Type::FocusGained},
    };
    for (const auto& [source, expected] : mapping) {
        const std::vector<platform::Event> translated = translate(makeEvent(source));
        ASSERT_EQ(translated.size(), 1U) << source;
        EXPECT_EQ(translated[0].type, expected) << source;
    }
    EXPECT_TRUE(translate(makeEvent(SDL_EVENT_WINDOW_MOVED)).empty());
    EXPECT_TRUE(translate(makeEvent(SDL_EVENT_DROP_FILE)).empty());
}

TEST_F(EventsTest, ReportsEachStateOfTheAppOnce) {
    EXPECT_TRUE(translate(makeEvent(SDL_EVENT_DID_ENTER_FOREGROUND)).empty());
    EXPECT_EQ(translate(makeEvent(SDL_EVENT_WILL_ENTER_BACKGROUND))[0].type, platform::Event::Type::FocusLost);
    EXPECT_TRUE(translate(makeEvent(SDL_EVENT_WILL_ENTER_BACKGROUND)).empty());
    EXPECT_EQ(translate(makeEvent(SDL_EVENT_DID_ENTER_FOREGROUND))[0].type, platform::Event::Type::FocusGained);
}

} // namespace haylen::platform::sdl
