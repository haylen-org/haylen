#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/input/ActionMap.hpp"
#include "haylen/input/ActionMaps.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/input/MotionSensors.hpp"
#include "haylen/input/VirtualInput.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/platform/Permission.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::input {

// Drives the motion sensors through the fake sensors of the headless host, which tests give sensors, answers and readings the way a device would.
class MotionSensorsTest : public ::testing::Test {
  protected:
    static constexpr float kGravity = 9.80665F;

    [[nodiscard]] static MotionSensors::Sensors makeSensors(std::initializer_list<MotionSensor> list) {
        MotionSensors::Sensors sensors;
        for (const MotionSensor sensor : list) {
            sensors.set(static_cast<std::size_t>(sensor));
        }
        return sensors;
    }

    // The readings of an accelerometer whose device lies still, leaning from face up by the angles in degrees, the top edge down by `pitch` and then the right edge down by `roll`.
    [[nodiscard]] static MotionState makeLean(float roll, float pitch) {
        const float right = roll * std::numbers::pi_v<float> / 180.0F;
        const float top = pitch * std::numbers::pi_v<float> / 180.0F;
        MotionState state;
        state.sensors = makeSensors({MotionSensor::Accelerometer});
        state.acceleration = {-std::sin(right) * std::cos(top) * kGravity, -std::sin(top) * kGravity, std::cos(right) * std::cos(top) * kGravity};
        return state;
    }

    // Starts a request and keeps the answers that its callback receives.
    std::uint64_t start(MotionSensors& sensors, const MotionSensors::Options& options) {
        return sensors.start(options, [this](std::optional<MotionSensors::Failure> failure) { answers.push_back(std::move(failure)); });
    }

    void giveSensors(test::EngineFixture& fixture, std::initializer_list<MotionSensor> list) {
        fixture.host().setMotionSupport({.sensors = makeSensors(list)});
    }

    std::vector<std::optional<MotionSensors::Failure>> answers;
};

TEST_F(MotionSensorsTest, StayOffUntilARequestRunsAndAnswerInALaterFrame) {
    test::EngineFixture fixture;
    giveSensors(fixture, {MotionSensor::Accelerometer, MotionSensor::Gyroscope});
    fixture.frames(1);
    EXPECT_TRUE(fixture.host().getMotionSensors().none());

    MotionSensors& sensors = fixture.engine().getMotionSensors();
    const std::uint64_t id = start(sensors, {.sensors = makeSensors({MotionSensor::Accelerometer})});
    EXPECT_TRUE(answers.empty());
    EXPECT_FALSE(sensors.isRunning(id));

    fixture.frames(2);
    ASSERT_EQ(answers.size(), 1U);
    EXPECT_FALSE(answers.front().has_value());
    EXPECT_TRUE(sensors.isRunning(id));
    EXPECT_EQ(fixture.host().getMotionSensors(), makeSensors({MotionSensor::Accelerometer}));
    EXPECT_FLOAT_EQ(fixture.host().getMotionRate(), MotionSensors::kDefaultRate);
    EXPECT_EQ(std::ranges::count(fixture.host().getPermissionRequests(), platform::Permission::Kind::Motion), 1);
}

TEST_F(MotionSensorsTest, RunWhatEveryRequestWantsAtTheHighestRateAndStopWithTheLast) {
    test::EngineFixture fixture;
    giveSensors(fixture, {MotionSensor::Accelerometer, MotionSensor::Gyroscope, MotionSensor::Attitude});
    MotionSensors& sensors = fixture.engine().getMotionSensors();
    const std::uint64_t slow = start(sensors, {.sensors = makeSensors({MotionSensor::Accelerometer}), .rate = 30.0F});
    const std::uint64_t fast = start(sensors, {.sensors = makeSensors({MotionSensor::Gyroscope}), .rate = 100.0F});
    fixture.frames(2);
    EXPECT_EQ(fixture.host().getMotionSensors(), makeSensors({MotionSensor::Accelerometer, MotionSensor::Gyroscope}));
    EXPECT_FLOAT_EQ(fixture.host().getMotionRate(), 100.0F);

    sensors.stop(fast);
    fixture.frames(1);
    EXPECT_EQ(fixture.host().getMotionSensors(), makeSensors({MotionSensor::Accelerometer}));
    EXPECT_FLOAT_EQ(fixture.host().getMotionRate(), 30.0F);

    sensors.stop(slow);
    fixture.frames(1);
    EXPECT_TRUE(fixture.host().getMotionSensors().none());
    EXPECT_EQ(answers.size(), 2U);
}

TEST_F(MotionSensorsTest, FailSensorsThatTheDeviceLacksWithTheReason) {
    test::EngineFixture fixture;
    MotionSensors& sensors = fixture.engine().getMotionSensors();
    fixture.host().setMotionSupport({.reason = "The test device has no sensors at all."});
    start(sensors, {.sensors = makeSensors({MotionSensor::Accelerometer})});

    giveSensors(fixture, {MotionSensor::Accelerometer});
    start(sensors, {.sensors = makeSensors({MotionSensor::Gyroscope, MotionSensor::Attitude})});
    start(sensors, {.sensors = makeSensors({MotionSensor::Accelerometer, MotionSensor::Gyroscope})});
    fixture.frames(1);

    ASSERT_EQ(answers.size(), 3U);
    EXPECT_EQ(answers[0]->code, "unsupported");
    EXPECT_EQ(answers[0]->message, "The test device has no sensors at all.");
    EXPECT_EQ(answers[1]->message, "The device has no motion sensors \"gyroscope\" and \"attitude\".");
    EXPECT_EQ(answers[2]->message, "The device has no motion sensor \"gyroscope\".");
    EXPECT_EQ(std::ranges::count(fixture.host().getPermissionRequests(), platform::Permission::Kind::Motion), 0);
    EXPECT_TRUE(fixture.host().getMotionSensors().none());
}

TEST_F(MotionSensorsTest, FailWithTheAnswerOfThePlatform) {
    test::EngineFixture fixture;
    giveSensors(fixture, {MotionSensor::Accelerometer});
    MotionSensors& sensors = fixture.engine().getMotionSensors();
    const MotionSensors::Options options{.sensors = makeSensors({MotionSensor::Accelerometer})};

    fixture.host().setPermissionAnswer({.state = platform::Permission::State::Denied});
    start(sensors, options);
    fixture.frames(1);
    fixture.host().setPermissionAnswer({.state = platform::Permission::State::Unsupported, .missing = {{.kind = "usageDescription", .name = "NSMotionUsageDescription"}}, .message = "The motion sensors need a usage description."});
    start(sensors, options);
    fixture.frames(1);
    fixture.host().setPermissionAnswer({.state = platform::Permission::State::Prompt, .message = "The browser asks only during a tap."});
    start(sensors, options);
    fixture.frames(1);

    ASSERT_EQ(answers.size(), 3U);
    EXPECT_EQ(answers[0]->code, "denied");
    EXPECT_FALSE(answers[0]->message.empty());
    EXPECT_EQ(answers[1]->code, "unsupported");
    ASSERT_EQ(answers[1]->missing.size(), 1U);
    EXPECT_EQ(answers[1]->missing.front().name, "NSMotionUsageDescription");
    EXPECT_EQ(answers[2]->code, "failed");
    EXPECT_EQ(answers[2]->message, "The browser asks only during a tap.");
    EXPECT_TRUE(fixture.host().getMotionSensors().none());
}

TEST_F(MotionSensorsTest, CancelARequestThatStopsBeforeItsAnswer) {
    test::EngineFixture fixture;
    giveSensors(fixture, {MotionSensor::Accelerometer});
    fixture.host().setPermissionAnswersHeld(true);
    MotionSensors& sensors = fixture.engine().getMotionSensors();
    const std::uint64_t id = start(sensors, {.sensors = makeSensors({MotionSensor::Accelerometer})});
    fixture.frames(2);
    EXPECT_TRUE(answers.empty());

    sensors.stop(id);
    fixture.host().answerPermissions();
    fixture.frames(2);
    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front()->code, "cancelled");
    EXPECT_TRUE(fixture.host().getMotionSensors().none());
}

TEST_F(MotionSensorsTest, StopInTheBackgroundAndRunAgainInFront) {
    test::EngineFixture fixture;
    giveSensors(fixture, {MotionSensor::Accelerometer});
    start(fixture.engine().getMotionSensors(), {.sensors = makeSensors({MotionSensor::Accelerometer}), .gamepads = true});
    fixture.frames(2);
    EXPECT_TRUE(fixture.host().getMotionSensors().any());
    EXPECT_TRUE(fixture.host().getGamepadMotion().any());

    fixture.engine().handleEvent({.type = platform::Event::Type::Suspended});
    fixture.frames(1);
    EXPECT_TRUE(fixture.host().getMotionSensors().none());
    EXPECT_TRUE(fixture.host().getGamepadMotion().none());

    fixture.engine().handleEvent({.type = platform::Event::Type::Resumed});
    fixture.frames(1);
    EXPECT_EQ(fixture.host().getMotionSensors(), makeSensors({MotionSensor::Accelerometer}));
    EXPECT_EQ(fixture.host().getGamepadMotion(), makeSensors({MotionSensor::Accelerometer}));
}

TEST_F(MotionSensorsTest, RunTheSensorsOfGamepadsWithoutAsking) {
    test::EngineFixture fixture;
    MotionSensors& sensors = fixture.engine().getMotionSensors();
    const std::uint64_t id = start(sensors, {.sensors = makeSensors({MotionSensor::Accelerometer, MotionSensor::Gyroscope}), .device = false, .gamepads = true});
    fixture.frames(1);
    ASSERT_EQ(answers.size(), 1U);
    EXPECT_FALSE(answers.front().has_value());
    EXPECT_EQ(std::ranges::count(fixture.host().getPermissionRequests(), platform::Permission::Kind::Motion), 0);
    EXPECT_TRUE(fixture.host().getMotionSensors().none());
    EXPECT_EQ(fixture.host().getGamepadMotion(), makeSensors({MotionSensor::Accelerometer, MotionSensor::Gyroscope}));

    sensors.stop(id);
    fixture.frames(1);
    EXPECT_TRUE(fixture.host().getGamepadMotion().none());
}

TEST_F(MotionSensorsTest, RejectRequestsThatAskForNothingOrAtAnImpossibleRate) {
    test::EngineFixture fixture;
    MotionSensors& sensors = fixture.engine().getMotionSensors();
    const MotionSensors::Sensors accelerometer = makeSensors({MotionSensor::Accelerometer});
    EXPECT_THROW(start(sensors, {}), std::invalid_argument);
    EXPECT_THROW(start(sensors, {.sensors = accelerometer, .device = false}), std::invalid_argument);
    for (const float rate : {0.0F, 201.0F, std::numeric_limits<float>::quiet_NaN()}) {
        EXPECT_THROW(start(sensors, {.sensors = accelerometer, .rate = rate}), std::invalid_argument);
    }
}

TEST_F(MotionSensorsTest, StopWhatRunsWhenTheAppEnds) {
    test::EngineFixture fixture;
    giveSensors(fixture, {MotionSensor::Accelerometer});
    start(fixture.engine().getMotionSensors(), {.sensors = makeSensors({MotionSensor::Accelerometer}), .gamepads = true});
    fixture.frames(2);
    EXPECT_TRUE(fixture.host().getMotionSensors().any());

    fixture.restart();
    EXPECT_TRUE(fixture.host().getMotionSensors().none());
    EXPECT_TRUE(fixture.host().getGamepadMotion().none());
}

TEST_F(MotionSensorsTest, MeasureTheTiltFromTheGravityOfTheAccelerometer) {
    Input input;
    EXPECT_EQ(input.getTilt(), math::Vec2{});

    input.updateMotion(makeLean(0.0F, 0.0F));
    EXPECT_NEAR(input.getTilt().getLength(), 0.0F, 1.0e-6F);

    // The right edge down rolls a ball to the right, and the top edge down rolls it up the screen.
    input.updateMotion(makeLean(30.0F, 0.0F));
    EXPECT_NEAR(input.getTilt().x, 0.5F, 1.0e-5F);
    EXPECT_NEAR(input.getTilt().y, 0.0F, 1.0e-5F);
    input.updateMotion(makeLean(0.0F, 30.0F));
    EXPECT_NEAR(input.getTilt().y, -0.5F, 1.0e-5F);

    // A device held upright pulls the ball to the bottom of the screen until it becomes level, and then rolls it with its right edge down.
    input.updateMotion(makeLean(0.0F, -90.0F));
    EXPECT_NEAR(input.getTilt().y, 1.0F, 1.0e-5F);
    EXPECT_TRUE(input.calibrateTilt());
    EXPECT_NEAR(input.getTilt().getLength(), 0.0F, 1.0e-5F);
    MotionState upright = makeLean(0.0F, 0.0F);
    upright.acceleration = {-0.5F * kGravity, std::cos(std::numbers::pi_v<float> / 6.0F) * kGravity, 0.0F};
    input.updateMotion(upright);
    EXPECT_NEAR(input.getTilt().x, 0.5F, 1.0e-5F);
    EXPECT_NEAR(input.getTilt().y, 0.0F, 1.0e-5F);

    input.resetTilt();
    EXPECT_NEAR(input.getTilt().y, std::cos(30.0F * std::numbers::pi_v<float> / 180.0F), 1.0e-5F);

    // Without readings of the accelerometer the tilt is zero and calibrating changes nothing.
    input.updateMotion({});
    EXPECT_EQ(input.getTilt(), math::Vec2{});
    EXPECT_FALSE(input.calibrateTilt());
}

TEST_F(MotionSensorsTest, MeasureTheTiltOfAGamepadAndClearItsTouchpadsWhenItGoes) {
    Input input;
    GamepadState gamepad;
    gamepad.connected = true;
    gamepad.motion = makeLean(-30.0F, 0.0F);
    gamepad.touchpad[0] = {.down = true, .position = {0.25F, 0.75F}, .pressure = 0.5F};
    input.updateGamepads(std::array<GamepadState, 1>{gamepad});
    EXPECT_NEAR(input.getGamepadTilt(0).x, -0.5F, 1.0e-5F);
    EXPECT_TRUE(input.getGamepad(0).touchpad[0].down);
    EXPECT_EQ(input.getGamepadTilt(Input::kMaxGamepads), math::Vec2{});

    input.updateGamepads({});
    EXPECT_FALSE(input.getGamepad(0).touchpad[0].down);
    EXPECT_EQ(input.getGamepadTilt(0), math::Vec2{});
}

TEST_F(MotionSensorsTest, ReadTheTiltAsBindingsOfTheActionMap) {
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
    input.updateMotion(makeLean(-30.0F, 0.0F));
    map.update(input, virtualInput, false);
    EXPECT_NEAR(map.getValue("steer"), -0.5F, 1.0e-5F);
    EXPECT_NEAR(map.getVector("roll").x, -0.5F, 1.0e-5F);
    EXPECT_FALSE(map.isDown("dive"));
    input.updateMotion(makeLean(0.0F, -60.0F));
    map.update(input, virtualInput, false);
    EXPECT_TRUE(map.isDown("dive"));

    // A map that leaves out the motion of the device reads the gamepads that lean instead.
    map.setDevices({.motion = false});
    GamepadState gamepad;
    gamepad.connected = true;
    gamepad.motion = makeLean(30.0F, 0.0F);
    input.updateGamepads(std::array<GamepadState, 1>{gamepad});
    map.update(input, virtualInput, false);
    EXPECT_NEAR(map.getValue("steer"), 0.5F, 1.0e-5F);
    EXPECT_FALSE(map.isDown("dive"));

    map.setDevices({.gamepads = ActionMap::Devices::Gamepads::None, .motion = false});
    map.update(input, virtualInput, false);
    EXPECT_FLOAT_EQ(map.getValue("steer"), 0.0F);
}

TEST_F(MotionSensorsTest, HandTheReadingsOfTheDeviceToTheInputAndTheActions) {
    test::EngineFixture fixture;
    giveSensors(fixture, {MotionSensor::Accelerometer, MotionSensor::Gyroscope});
    MotionState readings = makeLean(30.0F, 0.0F);
    readings.sensors.set(static_cast<std::size_t>(MotionSensor::Gyroscope));
    readings.rotationRate = {0.0F, 0.0F, 1.5F};
    fixture.host().setMotionReadings(readings);
    fixture.frames(1);
    EXPECT_TRUE(fixture.engine().getInput().getMotion().sensors.none());

    start(fixture.engine().getMotionSensors(), {.sensors = makeSensors({MotionSensor::Accelerometer})});
    fixture.engine().getActionMaps().getDefault()->define({.name = "steer", .type = ActionMap::Action::Type::Axis, .positive = {*ActionMap::Binding::parse("tilt:x+")}});
    fixture.frames(2);
    const MotionState& motion = fixture.engine().getInput().getMotion();
    EXPECT_EQ(motion.sensors, makeSensors({MotionSensor::Accelerometer}));
    EXPECT_FLOAT_EQ(motion.rotationRate[2], 0.0F);
    EXPECT_NEAR(fixture.engine().getActions().getValue("steer"), 0.5F, 1.0e-5F);
}

TEST_F(MotionSensorsTest, StartFromLuaAndStopWithTheOwner) {
    test::EngineFixture fixture;
    giveSensors(fixture, {MotionSensor::Accelerometer, MotionSensor::Gyroscope});
    MotionState readings = makeLean(30.0F, 0.0F);
    readings.sensors.set(static_cast<std::size_t>(MotionSensor::Gyroscope));
    readings.rotationRate = {0.25F, 0.5F, 1.0F};
    fixture.host().setMotionReadings(readings);

    // clang-format off
    fixture.runLua(R"(
        input = require('haylen.input')
        async = require('async')
        owner = {}
        async.spawn(function()
            request = input.startMotion({sensors = {'accelerometer', 'gyroscope'}, rate = 50, owner = owner}):await()
        end)
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return tostring(request ~= nil)") == "true"; }));
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return tostring(request.running) .. ' ' .. table.concat(request.sensors, ',') .. ' ' .. request.rate .. ' ' .. tostring(request.device) .. ' ' .. tostring(request.gamepads)"), "true accelerometer,gyroscope 50.0 true false");
    EXPECT_EQ(fixture.lua("return table.concat(input.motionSensors(), ',')"), "accelerometer,gyroscope");
    EXPECT_EQ(fixture.lua("local x, y, z = input.rotationRate() return x .. ' ' .. y .. ' ' .. z"), "0.25 0.5 1.0");
    EXPECT_EQ(fixture.lua("local x, y = input.tilt() return string.format('%.2f %.2f', x, y)"), "0.50 0.00");
    EXPECT_EQ(fixture.lua("local x, y, z, w = input.attitude() return x .. ' ' .. y .. ' ' .. z .. ' ' .. w"), "0.0 0.0 0.0 1.0");
    EXPECT_EQ(fixture.lua("local _, _, z = input.acceleration() return string.format('%.2f', z)"), "8.49");
    EXPECT_EQ(fixture.lua("return tostring(input.calibrateTilt())"), "true");
    EXPECT_EQ(fixture.lua("local x, y = input.tilt() return tostring(math.abs(x) + math.abs(y) < 1e-4)"), "true");
    fixture.runLua("input.resetTilt()");
    EXPECT_FLOAT_EQ(fixture.host().getMotionRate(), 50.0F);

    fixture.runLua("owner = nil collectgarbage() collectgarbage()");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.host().getMotionSensors().none(); }));
    EXPECT_EQ(fixture.lua("return tostring(request.running)"), "false");
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

TEST_F(MotionSensorsTest, FailTheCallFromLuaWithItsCode) {
    test::EngineFixture fixture;
    fixture.host().setMotionSupport({.reason = "The test device has no sensors at all."});
    // clang-format off
    fixture.runLua(R"(
        input = require('haylen.input')
        async = require('async')
        async.spawn(function()
            local _, failure = input.startMotion():await()
            reason = failure
        end)
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return type(reason)") == "table"; }));
    EXPECT_EQ(fixture.lua("return reason.code .. ' ' .. reason.message"), "unsupported The test device has no sensors at all.");

    fixture.host().setMotionSupport({.sensors = makeSensors({MotionSensor::Accelerometer})});
    fixture.host().setPermissionAnswer({.state = platform::Permission::State::Unsupported, .missing = {{.kind = "usageDescription", .name = "NSMotionUsageDescription", .file = "ios/Info.plist", .snippet = "<key>NSMotionUsageDescription</key>"}}, .message = "The motion sensors need a usage description."});
    fixture.runLua("async.spawn(function() local _, failure = input.startMotion():await() missing = failure end)");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return type(missing)") == "table"; }));
    EXPECT_EQ(fixture.lua("return missing.code .. ' ' .. missing.data.missing[1].name"), "unsupported NSMotionUsageDescription");

    // A call that the app gives up before its answer stops the request.
    fixture.host().setPermissionAnswer({.state = platform::Permission::State::Granted});
    fixture.host().setPermissionAnswersHeld(true);
    fixture.runLua("waiting = input.startMotion() async.spawn(function() local _, failure = waiting:await() given = failure end)");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return tostring(waiting:cancel())"), "true");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return type(given)") == "table"; }));
    EXPECT_EQ(fixture.lua("return given.code .. ' ' .. tostring(waiting:cancel())"), "cancelled false");

    EXPECT_NE(fixture.lua("input.startMotion({sensors = {'compass'}})").find("compass"), std::string::npos);
    EXPECT_NE(fixture.lua("input.startMotion({speed = 2})").find("Unknown option \"speed\"."), std::string::npos);
    EXPECT_NE(fixture.lua("input.startMotion({rate = 500})").find("from 1 to 200"), std::string::npos);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

TEST_F(MotionSensorsTest, ReadTheMotionAndTouchpadsOfGamepadsFromLua) {
    test::EngineFixture fixture;
    GamepadState gamepad;
    gamepad.connected = true;
    gamepad.motionSensors = makeSensors({MotionSensor::Accelerometer, MotionSensor::Gyroscope});
    gamepad.motion = makeLean(30.0F, 0.0F);
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
