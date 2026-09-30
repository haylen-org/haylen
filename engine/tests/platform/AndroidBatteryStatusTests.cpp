#include <gtest/gtest.h>

#include "platform/android/AndroidBatteryStatus.hpp"

namespace haylen::platform {

TEST(AndroidBatteryStatusTest, ReadsTheBatteryOfTheBroadcast) {
    EXPECT_EQ(AndroidBatteryStatus::toBattery(false, -1, -1, 1), Battery{.state = Battery::State::None});
    EXPECT_EQ(AndroidBatteryStatus::toBattery(true, 42, 100, 3), (Battery{.level = 0.42F, .state = Battery::State::Discharging}));
    EXPECT_EQ(AndroidBatteryStatus::toBattery(true, 64, 100, 2), (Battery{.level = 0.64F, .charging = true, .state = Battery::State::Charging}));
    EXPECT_EQ(AndroidBatteryStatus::toBattery(true, 100, 100, 5), (Battery{.level = 1.0F, .state = Battery::State::Full}));

    // A plugged battery that Android keeps from charging does not charge, and the level follows the scale of the device.
    EXPECT_EQ(AndroidBatteryStatus::toBattery(true, 40, 50, 4), (Battery{.level = 0.8F, .state = Battery::State::Discharging}));

    // An unknown status or level leaves the state or the level unknown.
    EXPECT_EQ(AndroidBatteryStatus::toBattery(true, 90, 100, 1), (Battery{.level = 0.9F, .state = Battery::State::Unknown}));
    EXPECT_EQ(AndroidBatteryStatus::toBattery(true, -1, 100, 3), Battery{.state = Battery::State::Discharging});
    EXPECT_EQ(AndroidBatteryStatus::toBattery(true, 50, 0, 3), Battery{.state = Battery::State::Discharging});
}

} // namespace haylen::platform
