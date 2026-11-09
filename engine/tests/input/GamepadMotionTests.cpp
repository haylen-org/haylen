#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <initializer_list>
#include <numbers>
#include <stdexcept>
#include <string>

#include "haylen/input/ActionMap.hpp"
#include "haylen/input/ActionMaps.hpp"
#include "haylen/input/GamepadMotion.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/input/VirtualInput.hpp"
#include "haylen/platform/Event.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::input {

// Drives the motion sensors of gamepads through the headless host, which records the sensors that run and takes gamepads with the readings that tests give them.
class GamepadMotionTest : public ::testing::Test {
  protected:
    static constexpr float kGravity = 9.80665F;

    [[nodiscard]] static GamepadMotion::Sensors makeSensors(std::initializer_list<MotionSensor> list) {
        GamepadMotion::Sensors sensors;
        for (const MotionSensor sensor : list) {
            sensors.set(static_cast<std::size_t>(sensor));
        }
        return sensors;
    }

    // A connected gamepad whose accelerometer reads it lying still, leaning from flat by the angles in degrees, the far side down by `pitch` and then the right side down by `roll`.
    [[nodiscard]] static GamepadState makeLeaning(float roll, float pitch) {
        const float right = roll * std::numbers::pi_v<float> / 180.0F;
        const float far = pitch * std::numbers::pi_v<float> / 180.0F;
        GamepadState gamepad;
        gamepad.connected = true;
        gamepad.motionSensors = makeSensors({MotionSensor::Accelerometer});
        gamepad.motion.sensors = gamepad.motionSensors;
        gamepad.motion.acceleration = {-std::sin(right) * std::cos(far) * kGravity, -std::sin(far) * kGravity, std::cos(right) * std::cos(far) * kGravity};
        return gamepad;
    }
};

TEST_F(GamepadMotionTest, RunWhatEveryRequestWantsAndStopWithTheLast) {
    test::EngineFixture fixture;
    fixture.frames(1);
    EXPECT_TRUE(fixture.host().getGamepadMotion().none());

    GamepadMotion& motion = fixture.engine().getGamepadMotion();
    const std::uint64_t tilt = motion.start(makeSensors({MotionSensor::Accelerometer}));
    const std::uint64_t aim = motion.start(makeSensors({MotionSensor::Gyroscope}));
    EXPECT_TRUE(motion.isRunning(tilt));
    fixture.frames(1);
    EXPECT_EQ(fixture.host().getGamepadMotion(), makeSensors({MotionSensor::Accelerometer, MotionSensor::Gyroscope}));

    motion.stop(aim);
    fixture.frames(1);
    EXPECT_FALSE(motion.isRunning(aim));
    EXPECT_EQ(fixture.host().getGamepadMotion(), makeSensors({MotionSensor::Accelerometer}));

    motion.stop(tilt);
    fixture.frames(1);
    EXPECT_TRUE(fixture.host().getGamepadMotion().none());
    EXPECT_THROW(motion.start({}), std::invalid_argument);
}

TEST_F(GamepadMotionTest, StopInTheBackgroundAndRunAgainInFront) {
    test::EngineFixture fixture;
    fixture.engine().getGamepadMotion().start(makeSensors({MotionSensor::Accelerometer}));
    fixture.frames(1);
    EXPECT_EQ(fixture.host().getGamepadMotion(), makeSensors({MotionSensor::Accelerometer}));

    fixture.engine().handleEvent({.type = platform::Event::Type::Suspended});
    fixture.frames(1);
    EXPECT_TRUE(fixture.host().getGamepadMotion().none());

    fixture.engine().handleEvent({.type = platform::Event::Type::Resumed});
    fixture.frames(1);
    EXPECT_EQ(fixture.host().getGamepadMotion(), makeSensors({MotionSensor::Accelerometer}));
}

TEST_F(GamepadMotionTest, StopWhatRunsWhenTheAppEnds) {
    test::EngineFixture fixture;
    fixture.engine().getGamepadMotion().start(makeSensors({MotionSensor::Gyroscope}));
    fixture.frames(1);
    EXPECT_TRUE(fixture.host().getGamepadMotion().any());

    fixture.restart();
    EXPECT_TRUE(fixture.host().getGamepadMotion().none());
}

TEST_F(GamepadMotionTest, MeasureTheTiltOfAGamepadAndClearItsTouchpadsWhenItGoes) {
    Input input;
    EXPECT_EQ(input.getGamepadTilt(0), math::Vec2{});

    // The right side down rolls a ball to the right, and the far side down rolls it up the screen.
    GamepadState gamepad = makeLeaning(30.0F, 0.0F);
    gamepad.touchpad[0] = {.down = true, .position = {0.25F, 0.75F}, .pressure = 0.5F};
    input.updateGamepads(std::array<GamepadState, 1>{gamepad});
    EXPECT_NEAR(input.getGamepadTilt(0).x, 0.5F, 1.0e-5F);
    EXPECT_NEAR(input.getGamepadTilt(0).y, 0.0F, 1.0e-5F);
    EXPECT_TRUE(input.getGamepad(0).touchpad[0].down);
    EXPECT_EQ(input.getGamepadTilt(Input::kMaxGamepads), math::Vec2{});

    input.updateGamepads(std::array<GamepadState, 1>{makeLeaning(0.0F, 30.0F)});
    EXPECT_NEAR(input.getGamepadTilt(0).y, -0.5F, 1.0e-5F);

    input.updateGamepads({});
    EXPECT_FALSE(input.getGamepad(0).touchpad[0].down);
    EXPECT_EQ(input.getGamepadTilt(0), math::Vec2{});
}

TEST_F(GamepadMotionTest, ReadTheTiltAsBindingsOfTheActionMap) {
    EXPECT_EQ(ActionMap::Binding::parse("tilt:x+")->toString(), "tilt:x+");
    EXPECT_EQ(ActionMap::Binding::parse("tilt:y-")->toString(), "tilt:y-");
    EXPECT_EQ(ActionMap::Binding::parse("stick:tilt")->toString(), "stick:tilt");
    for (const char* text : {"tilt:z+", "tilt:x", "tilt:xy+", "stick:middle"}) {
        EXPECT_FALSE(ActionMap::Binding::parse(text).has_value()) << text;
    }

    ActionMap map;
    map.load(core::Json::parse(R"({"actions": [
        {"name": "steer", "type": "axis", "positive": ["tilt:x+"], "negative": ["tilt:x-"]},
        {"name": "roll", "type": "vector", "bindings": ["stick:tilt"]},
        {"name": "dive", "type": "button", "bindings": ["tilt:y+"]}
    ]})"));
    Input input;
    const VirtualInput virtualInput;
    input.updateGamepads(std::array<GamepadState, 2>{makeLeaning(-30.0F, 0.0F), makeLeaning(10.0F, 0.0F)});
    map.update(input, virtualInput, false);
    EXPECT_NEAR(map.getValue("steer"), -0.5F, 1.0e-5F);
    EXPECT_NEAR(map.getVector("roll").x, -0.5F, 1.0e-5F);
    EXPECT_FALSE(map.isDown("dive"));

    input.updateGamepads(std::array<GamepadState, 1>{makeLeaning(0.0F, -60.0F)});
    map.update(input, virtualInput, false);
    EXPECT_TRUE(map.isDown("dive"));

    // A map of one gamepad reads that gamepad alone, and a map of none reads no tilt.
    input.updateGamepads(std::array<GamepadState, 2>{makeLeaning(-30.0F, 0.0F), makeLeaning(10.0F, 0.0F)});
    map.setDevices({.gamepads = ActionMap::Devices::Gamepads::One, .gamepad = 1});
    map.update(input, virtualInput, false);
    EXPECT_NEAR(map.getValue("steer"), std::sin(10.0F * std::numbers::pi_v<float> / 180.0F), 1.0e-5F);

    map.setDevices({.gamepads = ActionMap::Devices::Gamepads::None});
    map.update(input, virtualInput, false);
    EXPECT_FLOAT_EQ(map.getValue("steer"), 0.0F);
}

TEST_F(GamepadMotionTest, StartFromLuaAndStopWithTheOwner) {
    test::EngineFixture fixture;
    fixture.runLua("input = require('haylen.input') owner = {} request = input.startGamepadMotion({sensors = {'accelerometer', 'gyroscope'}, owner = owner})");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return tostring(request.running) .. ' ' .. table.concat(request.sensors, ',')"), "true accelerometer,gyroscope");
    EXPECT_EQ(fixture.host().getGamepadMotion(), makeSensors({MotionSensor::Accelerometer, MotionSensor::Gyroscope}));

    fixture.runLua("owner = nil collectgarbage() collectgarbage()");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.host().getGamepadMotion().none(); }));
    EXPECT_EQ(fixture.lua("return tostring(request.running)"), "false");

    fixture.runLua("request = input.startGamepadMotion()");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat(request.sensors, ',')"), "accelerometer");
    fixture.runLua("request:stop()");
    fixture.frames(1);
    EXPECT_TRUE(fixture.host().getGamepadMotion().none());

    EXPECT_NE(fixture.lua("input.startGamepadMotion({sensors = {'compass'}})").find("compass"), std::string::npos);
    EXPECT_NE(fixture.lua("input.startGamepadMotion({sensors = {}})").find("The motion of gamepads needs at least one sensor, such as \"accelerometer\"."), std::string::npos);
    EXPECT_NE(fixture.lua("input.startGamepadMotion({rate = 60})").find("Unknown option \"rate\"."), std::string::npos);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

TEST_F(GamepadMotionTest, ReadTheMotionAndTouchpadsOfGamepadsFromLua) {
    test::EngineFixture fixture;
    GamepadState gamepad = makeLeaning(30.0F, 0.0F);
    gamepad.motionSensors.set(static_cast<std::size_t>(MotionSensor::Gyroscope));
    gamepad.motion.sensors.set(static_cast<std::size_t>(MotionSensor::Gyroscope));
    gamepad.motion.rotationRate = {1.0F, 2.0F, 3.0F};
    gamepad.touchpad[1] = {.down = true, .touchpad = 0, .position = {0.25F, 0.5F}, .pressure = 0.75F};
    fixture.host().setGamepad(1, gamepad);
    fixture.frames(1);

    fixture.runLua("input = require('haylen.input')");
    EXPECT_EQ(fixture.lua("return table.concat(input.gamepadSensors(2), ',') .. ' ' .. #input.gamepadSensors(1)"), "accelerometer,gyroscope 0");
    EXPECT_EQ(fixture.lua("local x, y, z = input.gamepadRotationRate(2) return x .. ' ' .. y .. ' ' .. z"), "1.0 2.0 3.0");
    EXPECT_EQ(fixture.lua("local x = input.gamepadTilt(2) return string.format('%.2f', x)"), "0.50");
    EXPECT_EQ(fixture.lua("local _, _, _, w = input.gamepadAttitude(2) return w"), "1.0");
    EXPECT_EQ(fixture.lua("local _, _, z = input.gamepadAcceleration(2) return string.format('%.2f', z)"), "8.49");
    EXPECT_EQ(fixture.lua("local list = input.gamepadTouches(2) local finger = list[1] return #list .. ' ' .. finger.finger .. ' ' .. finger.touchpad .. ' ' .. finger.x .. ' ' .. finger.y .. ' ' .. finger.pressure"), "1 2 1 0.25 0.5 0.75");

    fixture.host().setGamepad(1, {});
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return #input.gamepadTouches(2)"), "0");
}

} // namespace haylen::input
