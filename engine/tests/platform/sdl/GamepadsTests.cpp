#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <vector>

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_joystick.h>

#include "haylen/input/Input.hpp"
#include "platform/sdl/Gamepads.hpp"

namespace haylen::platform::sdl {

// SDL starts its gamepad subsystem without a window, and its virtual controllers stand in for real ones, with the vendor and product of a real controller where the labels matter.
class GamepadsTest : public ::testing::Test {
  protected:
    struct Motors {
        Uint16 low = 0;
        Uint16 high = 0;
    };

    void SetUp() override {
        ASSERT_TRUE(SDL_Init(SDL_INIT_GAMEPAD)) << SDL_GetError();
    }

    void TearDown() override {
        for (SDL_Joystick* joystick : joysticks) {
            SDL_CloseJoystick(joystick);
        }
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    }

    SDL_Joystick* attach(const char* name, Uint16 vendor, Uint16 product, Motors* motors = nullptr) {
        SDL_VirtualJoystickDesc desc;
        SDL_INIT_INTERFACE(&desc);
        desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        desc.vendor_id = vendor;
        desc.product_id = product;
        desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
        desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        desc.name = name;
        desc.userdata = motors;
        desc.Rumble = &GamepadsTest::rumble;
        SDL_Joystick* joystick = SDL_OpenJoystick(SDL_AttachVirtualJoystick(&desc));
        joysticks.push_back(joystick);
        return joystick;
    }

    void detach(SDL_Joystick* joystick) {
        const SDL_JoystickID id = SDL_GetJoystickID(joystick);
        std::erase(joysticks, joystick);
        SDL_CloseJoystick(joystick);
        SDL_DetachVirtualJoystick(id);
    }

    void pump() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            gamepads.handle(event);
        }
        gamepads.poll(states);
    }

    static bool SDLCALL rumble(void* userdata, Uint16 low, Uint16 high) {
        auto* motors = static_cast<Motors*>(userdata);
        motors->low = low;
        motors->high = high;
        return true;
    }

    Gamepads gamepads;
    std::array<input::GamepadState, input::Input::kMaxGamepads> states{};
    std::vector<SDL_Joystick*> joysticks;
};

TEST_F(GamepadsTest, KeepsEveryControllerInItsSlotWhileItStaysConnected) {
    SDL_Joystick* first = attach("First Controller", 0x045E, 0x0B12);
    attach("Second Controller", 0x054C, 0x09CC);
    pump();
    ASSERT_TRUE(states[0].connected);
    ASSERT_TRUE(states[1].connected);
    EXPECT_EQ(states[1].name, "Second Controller");
    EXPECT_EQ(states[0].labels, input::GamepadLabels::Letters);
    EXPECT_EQ(states[1].labels, input::GamepadLabels::Shapes);

    detach(first);
    pump();
    EXPECT_FALSE(states[0].connected);
    EXPECT_TRUE(states[1].connected);

    // A controller that connects takes the first free slot and leaves the others where they are.
    attach("Third Controller", 0x057E, 0x2009);
    pump();
    EXPECT_EQ(states[0].name, "Third Controller");
    EXPECT_EQ(states[0].labels, input::GamepadLabels::SwappedLetters);
    EXPECT_EQ(states[1].name, "Second Controller");
    EXPECT_FALSE(states[2].connected);
}

TEST_F(GamepadsTest, ReadsTheButtonsTheSticksAndTheTriggersOfAController) {
    SDL_Joystick* joystick = attach("Controller", 0x045E, 0x0B12);
    pump();
    SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, true);
    SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_DPAD_LEFT, true);
    SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFTY, SDL_JOYSTICK_AXIS_MAX);
    SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_RIGHTX, SDL_JOYSTICK_AXIS_MIN);
    SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, SDL_JOYSTICK_AXIS_MAX);
    pump();

    const input::GamepadState& state = states[0];
    EXPECT_TRUE(state.buttons[static_cast<std::size_t>(input::GamepadButton::South)]);
    EXPECT_TRUE(state.buttons[static_cast<std::size_t>(input::GamepadButton::DpadLeft)]);
    EXPECT_FALSE(state.buttons[static_cast<std::size_t>(input::GamepadButton::East)]);
    EXPECT_FLOAT_EQ(state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftY)], 1.0F);
    EXPECT_FLOAT_EQ(state.axes[static_cast<std::size_t>(input::GamepadAxis::RightX)], -1.0F);
    EXPECT_FLOAT_EQ(state.axes[static_cast<std::size_t>(input::GamepadAxis::RightTrigger)], 1.0F);
    EXPECT_FLOAT_EQ(state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftTrigger)], 0.0F);
}

TEST_F(GamepadsTest, RumblesTheMotorsOfTheControllerInTheSlot) {
    Motors motors;
    attach("Controller", 0x045E, 0x0B12, &motors);
    pump();

    gamepads.rumble(0, 1.0F, 0.5F, 0.25F);
    EXPECT_EQ(motors.low, 65535);
    EXPECT_EQ(motors.high, 32768);

    gamepads.rumble(0, 0.0F, 0.0F, 0.0F);
    EXPECT_EQ(motors.low, 0);
    EXPECT_EQ(motors.high, 0);

    // A slot without a controller rumbles nothing.
    gamepads.rumble(3, 1.0F, 1.0F, 1.0F);
    EXPECT_EQ(motors.low, 0);
}

} // namespace haylen::platform::sdl
