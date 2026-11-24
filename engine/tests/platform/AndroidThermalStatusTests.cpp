#include <gtest/gtest.h>

#include "platform/android/AndroidThermalStatus.hpp"

namespace haylen::platform {

TEST(AndroidThermalStatusTest, ReadsTheThermalStatusOfThePowerManager) {
    EXPECT_EQ(AndroidThermalStatus::toThermalState(0), ThermalState::Nominal);
    EXPECT_EQ(AndroidThermalStatus::toThermalState(1), ThermalState::Fair);
    EXPECT_EQ(AndroidThermalStatus::toThermalState(2), ThermalState::Fair);
    EXPECT_EQ(AndroidThermalStatus::toThermalState(3), ThermalState::Serious);
    EXPECT_EQ(AndroidThermalStatus::toThermalState(4), ThermalState::Critical);
    EXPECT_EQ(AndroidThermalStatus::toThermalState(5), ThermalState::Critical);
    EXPECT_EQ(AndroidThermalStatus::toThermalState(6), ThermalState::Critical);
    EXPECT_EQ(AndroidThermalStatus::toThermalState(7), ThermalState::Unknown);
    EXPECT_EQ(AndroidThermalStatus::toThermalState(-1), ThermalState::Unknown);
}

} // namespace haylen::platform
