#include <gtest/gtest.h>

#include "platform/windows/WindowsPowerStatus.hpp"

namespace haylen::platform {

TEST(WindowsPowerStatusTest, ReadsTheBatteryOfThePowerStatus) {
    EXPECT_EQ(WindowsPowerStatus::toBattery(1, 255, 255), Battery{});
    EXPECT_EQ(WindowsPowerStatus::toBattery(1, 128, 255), Battery{.state = Battery::State::None});
    EXPECT_EQ(WindowsPowerStatus::toBattery(0, 2, 18), (Battery{.level = 0.18F, .state = Battery::State::Discharging}));
    EXPECT_EQ(WindowsPowerStatus::toBattery(1, 1 | 8, 64), (Battery{.level = 0.64F, .charging = true, .state = Battery::State::Charging}));

    // A battery on mains power that does not charge is full, or held below full by the power settings.
    EXPECT_EQ(WindowsPowerStatus::toBattery(1, 1, 100), (Battery{.level = 1.0F, .state = Battery::State::Full}));
    EXPECT_EQ(WindowsPowerStatus::toBattery(1, 1, 80), (Battery{.level = 0.8F, .state = Battery::State::Full}));

    // An unknown line status or percentage leaves the state or the level unknown.
    EXPECT_EQ(WindowsPowerStatus::toBattery(255, 1, 90), (Battery{.level = 0.9F, .state = Battery::State::Unknown}));
    EXPECT_EQ(WindowsPowerStatus::toBattery(0, 0, 255), Battery{.state = Battery::State::Discharging});
}

} // namespace haylen::platform
