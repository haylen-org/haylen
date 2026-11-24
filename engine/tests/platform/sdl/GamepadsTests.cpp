#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_joystick.h>

#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/input/ActionMap.hpp"
#include "haylen/input/ActionMaps.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/platform/System.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "platform/sdl/Gamepads.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::platform::sdl {

// SDL starts its gamepad subsystem without a window, and its virtual controllers stand in for real ones, with the vendor and product of a real controller where the labels matter. A session hands the controllers to a running engine, the way the runtime polls them for every frame.
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

    // A controller with the motion sensors and the touchpad of the controllers that have them, whose sensors stay off until the app runs them.
    SDL_Joystick* attach(const char* name, Uint16 vendor, Uint16 product, Motors* motors = nullptr, bool sensors = false) {
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
        if (sensors) {
            desc.nsensors = static_cast<Uint16>(kSensors.size());
            desc.sensors = kSensors.data();
            desc.ntouchpads = 1;
            desc.touchpads = &kTouchpad;
            desc.SetSensorsEnabled = &GamepadsTest::enableSensors;
        }

        SDL_Joystick* joystick = SDL_OpenJoystick(SDL_AttachVirtualJoystick(&desc));
        if (joystick == nullptr) {
            throw std::runtime_error(std::string("SDL could not attach the virtual controller: ") + SDL_GetError());
        }

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

    // Plays frames of the engine with the controllers as SDL reads them now.
    void play(int frames = 1) {
        pump();
        for (std::size_t index = 0; index < states.size(); ++index) {
            session.host().setGamepad(index, states[index]);
        }

        session.frames(frames);
    }

    // A map of one player that reads only the controller in the slot, with the action `jump` on the south button.
    [[nodiscard]] std::shared_ptr<input::ActionMap> makePlayer(std::size_t slot) {
        std::shared_ptr<input::ActionMap> map = session.engine().getActionMaps().create({.keyboard = false, .mouse = false, .gamepads = input::ActionMap::Devices::Gamepads::One, .gamepad = slot, .virtualControls = false});
        map->define({.name = "jump", .type = input::ActionMap::Action::Type::Button, .bindings = {input::ActionMap::Binding{.source = input::ActionMap::Binding::Source::GamepadButton, .gamepadButton = input::GamepadButton::South}}});
        return map;
    }

    static bool SDLCALL enableSensors(void*, bool) {
        return true;
    }

    static bool SDLCALL rumble(void* userdata, Uint16 low, Uint16 high) {
        auto* motors = static_cast<Motors*>(userdata);
        motors->low = low;
        motors->high = high;
        return true;
    }

    static constexpr std::array<SDL_VirtualJoystickSensorDesc, 2> kSensors{{{SDL_SENSOR_ACCEL, 250.0F}, {SDL_SENSOR_GYRO, 250.0F}}};
    static constexpr SDL_VirtualJoystickTouchpadDesc kTouchpad{.nfingers = 2};

    test::EngineFixture session;
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
    ASSERT_TRUE(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, true));
    ASSERT_TRUE(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_DPAD_LEFT, true));
    ASSERT_TRUE(SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFTY, SDL_JOYSTICK_AXIS_MAX));
    ASSERT_TRUE(SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_RIGHTX, SDL_JOYSTICK_AXIS_MIN));
    ASSERT_TRUE(SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, SDL_JOYSTICK_AXIS_MAX));
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

// The sensors of a controller stay off until the app runs them, read with y out of the face of the controller as z of a face that lies flat, and the fingers on its touchpad go with the controller.
TEST_F(GamepadsTest, ReadsTheSensorsAndTheTouchpadOfAControllerThatHasThem) {
    SDL_Joystick* joystick = attach("Motion Controller", 0x054C, 0x0CE6, nullptr, true);
    pump();
    const std::array<float, 3> gravity{0.5F, 9.8F, -1.0F};
    ASSERT_TRUE(SDL_SendJoystickVirtualSensorData(joystick, SDL_SENSOR_ACCEL, 1, gravity.data(), static_cast<int>(gravity.size())));
    pump();
    EXPECT_TRUE(states[0].motionSensors.test(static_cast<std::size_t>(input::MotionSensor::Accelerometer)));
    EXPECT_TRUE(states[0].motionSensors.test(static_cast<std::size_t>(input::MotionSensor::Gyroscope)));
    EXPECT_FALSE(states[0].motionSensors.test(static_cast<std::size_t>(input::MotionSensor::Attitude)));
    EXPECT_TRUE(states[0].motion.sensors.none());

    input::GamepadMotion::Sensors wanted;
    wanted.set(static_cast<std::size_t>(input::MotionSensor::Accelerometer));
    gamepads.setMotion(wanted);
    ASSERT_TRUE(SDL_SendJoystickVirtualSensorData(joystick, SDL_SENSOR_ACCEL, 2, gravity.data(), static_cast<int>(gravity.size())));
    ASSERT_TRUE(SDL_SetJoystickVirtualTouchpad(joystick, 0, 1, true, 0.25F, 0.75F, 0.5F));
    pump();
    EXPECT_EQ(states[0].motion.sensors, wanted);
    EXPECT_EQ(states[0].motion.acceleration, (std::array<float, 3>{0.5F, 1.0F, 9.8F}));
    EXPECT_FALSE(states[0].touchpad[0].down);
    EXPECT_TRUE(states[0].touchpad[1].down);
    EXPECT_FLOAT_EQ(states[0].touchpad[1].position.x, 0.25F);
    EXPECT_FLOAT_EQ(states[0].touchpad[1].position.y, 0.75F);
    EXPECT_FLOAT_EQ(states[0].touchpad[1].pressure, 0.5F);

    // A controller that goes takes its readings and its fingers with it.
    detach(joystick);
    pump();
    EXPECT_FALSE(states[0].connected);
    EXPECT_FALSE(states[0].touchpad[1].down);
    EXPECT_TRUE(states[0].motion.sensors.none());
}

// Four players connect, play, leave and come back in the middle of a session, and every controller keeps its index, its events and the map of its player.
TEST_F(GamepadsTest, KeepsFourPlayersApartWhileTheirControllersComeAndGo) {
    std::vector<std::string> log;
    for (const std::string_view name : {core::LifecycleEvent::kGamepadConnected, core::LifecycleEvent::kGamepadDisconnected}) {
        session.engine().getEvents().on(name, [&log, name](core::EventBus::Event& event) { log.push_back(std::string(name) + " " + event.getData().dump()); });
    }

    // Controllers of one model share the name of that model, so the players hold four models to tell them apart.
    constexpr std::array<std::pair<Uint16, Uint16>, 4> kModels{{{0x045E, 0x0B12}, {0x054C, 0x09CC}, {0x054C, 0x0CE6}, {0x057E, 0x2009}}};
    std::array<SDL_Joystick*, 4> pads{};
    for (std::size_t index = 0; index < pads.size(); ++index) {
        pads[index] = attach(("Player " + std::to_string(index + 1)).c_str(), kModels[index].first, kModels[index].second);
    }

    std::array<std::shared_ptr<input::ActionMap>, 4> players;
    for (std::size_t slot = 0; slot < players.size(); ++slot) {
        players[slot] = makePlayer(slot);
    }

    play(2);
    EXPECT_EQ(log, (std::vector<std::string>{R"(gamepadConnected {"gamepad":1,"name":"Player 1"})", R"(gamepadConnected {"gamepad":2,"name":"Player 2"})", R"(gamepadConnected {"gamepad":3,"name":"Player 3"})", R"(gamepadConnected {"gamepad":4,"name":"Player 4"})"}));

    // Each controller presses for its own player alone.
    for (std::size_t presser = 0; presser < pads.size(); ++presser) {
        ASSERT_TRUE(SDL_SetJoystickVirtualButton(pads[presser], SDL_GAMEPAD_BUTTON_SOUTH, true));
        play();
        for (std::size_t slot = 0; slot < players.size(); ++slot) {
            EXPECT_EQ(players[slot]->isDown("jump"), slot == presser) << "The controller " << presser + 1 << " pressed for the player " << slot + 1 << ".";
        }

        ASSERT_TRUE(SDL_SetJoystickVirtualButton(pads[presser], SDL_GAMEPAD_BUTTON_SOUTH, false));
        play();
    }

    // The second player leaves while holding the button, which lets go of it, and the others keep their indices and their presses.
    ASSERT_TRUE(SDL_SetJoystickVirtualButton(pads[1], SDL_GAMEPAD_BUTTON_SOUTH, true));
    ASSERT_TRUE(SDL_SetJoystickVirtualButton(pads[3], SDL_GAMEPAD_BUTTON_SOUTH, true));
    play();
    ASSERT_TRUE(players[1]->isDown("jump"));
    log.clear();
    detach(pads[1]);
    play();
    EXPECT_EQ(log, (std::vector<std::string>{R"(gamepadDisconnected {"gamepad":2,"name":"Player 2"})"}));
    EXPECT_FALSE(players[1]->isDown("jump"));
    EXPECT_TRUE(players[3]->isDown("jump"));
    EXPECT_FALSE(session.engine().getInput().getGamepad(1).connected);
    EXPECT_EQ(session.engine().getInput().getGamepad(3).name, "Player 4");

    // A controller that joins takes the free index of the second player.
    log.clear();
    pads[1] = attach("Player 2 again", 0x0E6F, 0x0180);
    play();
    EXPECT_EQ(log, (std::vector<std::string>{R"(gamepadConnected {"gamepad":2,"name":"Player 2 again"})"}));
    ASSERT_TRUE(SDL_SetJoystickVirtualButton(pads[1], SDL_GAMEPAD_BUTTON_SOUTH, true));
    play();
    EXPECT_TRUE(players[1]->isPressed("jump"));
    EXPECT_FALSE(players[0]->isDown("jump"));
    EXPECT_TRUE(players[3]->isDown("jump"));
}

// The rumble that the app asks of a player reaches the motors of that controller alone, and the sensors of each controller read its own motion.
TEST_F(GamepadsTest, RumblesAndReadsTheMotionOfEachControllerOnItsOwn) {
    std::array<Motors, 4> motors;
    std::array<SDL_Joystick*, 4> pads{};
    for (std::size_t index = 0; index < pads.size(); ++index) {
        pads[index] = attach(("Player " + std::to_string(index + 1)).c_str(), 0x054C, 0x0CE6, &motors[index], true);
    }

    play();
    for (std::size_t slot = 0; slot < pads.size(); ++slot) {
        session.engine().getSystem().rumbleGamepad(slot, 0.25F * static_cast<float>(slot + 1), 0.0F, 0.5F);
    }

    // The engine hands each rumble to the platform with its index, which the controllers of SDL play in their slots.
    const std::vector<platform::HeadlessHost::Rumble>& asked = session.host().getRumbles();
    ASSERT_EQ(asked.size(), pads.size());
    for (const platform::HeadlessHost::Rumble& rumble : asked) {
        gamepads.rumble(rumble.index, rumble.low, rumble.high, rumble.seconds);
    }

    for (std::size_t slot = 0; slot < pads.size(); ++slot) {
        EXPECT_EQ(motors[slot].low, static_cast<Uint16>(std::lround(0.25F * static_cast<float>(slot + 1) * 65535.0F))) << "The controller " << slot + 1 << " rumbled with another strength.";
        EXPECT_EQ(motors[slot].high, 0);
    }

    input::GamepadMotion::Sensors wanted;
    wanted.set(static_cast<std::size_t>(input::MotionSensor::Accelerometer));
    gamepads.setMotion(wanted);
    for (std::size_t slot = 0; slot < pads.size(); ++slot) {
        const std::array<float, 3> gravity{static_cast<float>(slot), 9.8F, 0.0F};
        ASSERT_TRUE(SDL_SendJoystickVirtualSensorData(pads[slot], SDL_SENSOR_ACCEL, 1, gravity.data(), static_cast<int>(gravity.size())));
    }

    play();
    for (std::size_t slot = 0; slot < pads.size(); ++slot) {
        EXPECT_EQ(session.engine().getInput().getGamepad(slot).motion.acceleration, (std::array<float, 3>{static_cast<float>(slot), 0.0F, 9.8F})) << "The controller " << slot + 1 << " read the motion of another.";
    }

    // A controller that leaves stops nothing of the others.
    detach(pads[2]);
    play();
    gamepads.rumble(2, 1.0F, 1.0F, 1.0F);
    EXPECT_EQ(motors[3].low, static_cast<Uint16>(std::lround(65535.0F)));
    EXPECT_EQ(session.engine().getInput().getGamepad(3).motion.acceleration, (std::array<float, 3>{3.0F, 0.0F, 9.8F}));
}

} // namespace haylen::platform::sdl
