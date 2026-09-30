#pragma once

#include <cstdint>

#include "haylen/platform/Battery.hpp"

namespace haylen::platform {

// Reads the battery from the `SYSTEM_POWER_STATUS` of Windows. It compiles on every platform, so the tests of every host check it.
class WindowsPowerStatus final {
  public:
    // Takes `ACLineStatus`, `BatteryFlag` and `BatteryLifePercent`, where 255 means unknown.
    [[nodiscard]] static Battery toBattery(std::uint8_t lineStatus, std::uint8_t flags, std::uint8_t percent);

  private:
    static constexpr std::uint8_t kOffline = 0;
    static constexpr std::uint8_t kOnline = 1;
    static constexpr std::uint8_t kCharging = 8;
    static constexpr std::uint8_t kNoBattery = 128;
    static constexpr std::uint8_t kUnknown = 255;
};

} // namespace haylen::platform
