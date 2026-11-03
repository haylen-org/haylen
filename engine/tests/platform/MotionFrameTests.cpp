#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <numbers>

#include "platform/MotionFrame.hpp"

namespace haylen::platform {

class MotionFrameTest : public ::testing::Test {
  protected:
    static void expectNear(const std::array<float, 4>& actual, const std::array<float, 4>& expected) {
        for (std::size_t index = 0; index < actual.size(); ++index) {
            EXPECT_NEAR(actual[index], expected[index], 1.0e-5F) << index;
        }
    }
};

// A phone turned counterclockwise into landscape shows the app upright, so its own top edge becomes the left edge of the screen.
TEST_F(MotionFrameTest, TurnsTheAxesOfTheDeviceIntoThoseOfTheScreen) {
    input::MotionState state;
    state.acceleration = {1.0F, 2.0F, 3.0F};
    state.rotationRate = {0.5F, 0.0F, 1.0F};
    MotionFrame::turn(state, 1);
    EXPECT_EQ(state.acceleration, (std::array<float, 3>{-2.0F, 1.0F, 3.0F}));
    EXPECT_EQ(state.rotationRate, (std::array<float, 3>{0.0F, 0.5F, 1.0F}));

    MotionFrame::turn(state, 1);
    EXPECT_EQ(state.acceleration, (std::array<float, 3>{-1.0F, -2.0F, 3.0F}));
    MotionFrame::turn(state, -2);
    EXPECT_EQ(state.acceleration, (std::array<float, 3>{1.0F, 2.0F, 3.0F}));
    MotionFrame::turn(state, 3);
    EXPECT_EQ(state.acceleration, (std::array<float, 3>{2.0F, -1.0F, 3.0F}));

    // The attitude of a device that lies level turns the axes of the screen by the quarter turn back.
    input::MotionState level;
    MotionFrame::turn(level, 1);
    const float half = std::numbers::pi_v<float> / 4.0F;
    expectNear(level.attitude, {0.0F, 0.0F, -std::sin(half), std::cos(half)});
}

TEST_F(MotionFrameTest, ReadsTheAnglesOfBrowsers) {
    const float half = std::numbers::pi_v<float> / 4.0F;
    expectNear(MotionFrame::fromAngles(0.0, 0.0, 0.0), {0.0F, 0.0F, 0.0F, 1.0F});
    expectNear(MotionFrame::fromAngles(90.0, 0.0, 0.0), {0.0F, 0.0F, std::sin(half), std::cos(half)});
    expectNear(MotionFrame::fromAngles(0.0, 90.0, 0.0), {std::sin(half), 0.0F, 0.0F, std::cos(half)});
    expectNear(MotionFrame::fromAngles(0.0, 0.0, 90.0), {0.0F, std::sin(half), 0.0F, std::cos(half)});

    // Alpha turns first around z, and beta then turns around the new x.
    expectNear(MotionFrame::fromAngles(90.0, 90.0, 0.0), {0.5F, 0.5F, 0.5F, 0.5F});

    input::MotionState state;
    const std::array<float, 9> values{0.0F, 0.0F, 9.81F, 90.0F, 180.0F, 0.0F, 0.0F, 0.0F, 0.0F};
    std::bitset<input::Controls::kMotionSensorCount> sensors;
    sensors.set(static_cast<std::size_t>(input::MotionSensor::Accelerometer));
    sensors.set(static_cast<std::size_t>(input::MotionSensor::Gyroscope));
    MotionFrame::readBrowser(state, values, sensors, 0);
    EXPECT_EQ(state.sensors, sensors);
    EXPECT_FLOAT_EQ(state.acceleration[2], 9.81F);
    EXPECT_NEAR(state.rotationRate[0], std::numbers::pi_v<float> / 2.0F, 1.0e-5F);
    EXPECT_NEAR(state.rotationRate[1], std::numbers::pi_v<float>, 1.0e-5F);
    expectNear(state.attitude, {0.0F, 0.0F, 0.0F, 1.0F});
}

// A gamepad held flat points the y of its sensors up and their z toward the person, which becomes the z and the negative y of a face that lies flat.
TEST_F(MotionFrameTest, ReadsTheAxesOfGamepads) {
    EXPECT_EQ(MotionFrame::fromGamepadAxes({1.0F, 9.81F, 2.0F}), (std::array<float, 3>{1.0F, -2.0F, 9.81F}));
}

} // namespace haylen::platform
