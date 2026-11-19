#pragma once

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

#include "haylen/graphics/Viewport.hpp"
#include "haylen/input/Controls.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/platform/Event.hpp"

namespace haylen::input {

// Builds the platform events, viewports and gamepad states that drive the input tests.
class InputFixture : public ::testing::Test {
  protected:
    [[nodiscard]] static platform::Event makeKeyEvent(platform::Event::Type type, Key key, bool repeat = false) {
        platform::Event event;
        event.type = type;
        event.key = key;
        event.repeat = repeat;
        return event;
    }

    [[nodiscard]] static platform::Event makeMouseEvent(platform::Event::Type type, MouseButton button, math::Vec2 position = {}) {
        platform::Event event;
        event.type = type;
        event.mouseButton = button;
        event.position = position;
        return event;
    }

    [[nodiscard]] static platform::Event makeTouchEvent(platform::Event::Type type, std::uint64_t id, math::Vec2 position) {
        platform::Event event;
        event.type = type;
        event.touches[0] = platform::TouchPoint{id, position, true};
        event.touches[1] = platform::TouchPoint{id + 100, position, false};
        event.touchCount = 2;
        return event;
    }

    [[nodiscard]] static graphics::Viewport makeIdentityViewport() {
        graphics::Viewport viewport;
        viewport.update(math::Vec2(1920.0F, 1080.0F), math::Vec2(1920.0F, 1080.0F), graphics::Viewport::ScalingPolicy::Fit);
        return viewport;
    }

    [[nodiscard]] static GamepadState makeGamepad(GamepadButton button, float leftX = 0.0F) {
        GamepadState state;
        state.connected = true;
        state.buttons[static_cast<std::size_t>(button)] = true;
        state.axes[static_cast<std::size_t>(GamepadAxis::LeftX)] = leftX;
        return state;
    }

    // Returns the touch with an id, and throws when the input holds none, so a test never reads a touch that is not there.
    [[nodiscard]] static const Touch& touchOf(const Input& input, std::uint64_t id) {
        const Touch* touch = input.findTouch(id);
        if (touch == nullptr) {
            throw std::runtime_error("The input holds no touch " + std::to_string(id) + ".");
        }

        return *touch;
    }
};

} // namespace haylen::input
