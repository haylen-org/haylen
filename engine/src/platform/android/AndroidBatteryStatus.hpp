#pragma once

#include "haylen/platform/Battery.hpp"

namespace haylen::platform {

// Reads the battery from the sticky `ACTION_BATTERY_CHANGED` broadcast of Android. It compiles on every platform, so the tests of every host check it.
class AndroidBatteryStatus final {
  public:
    // Takes `EXTRA_PRESENT`, `EXTRA_LEVEL`, `EXTRA_SCALE` and `EXTRA_STATUS` of the broadcast, where a negative level or a scale that is not positive leaves the level unknown.
    [[nodiscard]] static Battery toBattery(bool present, int level, int scale, int status);

  private:
    // The values of `BatteryManager.BATTERY_STATUS_*`.
    static constexpr int kCharging = 2;
    static constexpr int kDischarging = 3;
    static constexpr int kNotCharging = 4;
    static constexpr int kFull = 5;
};

} // namespace haylen::platform
