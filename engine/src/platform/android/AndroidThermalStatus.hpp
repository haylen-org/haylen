#pragma once

#include "haylen/platform/ThermalState.hpp"

namespace haylen::platform {

// Reads the thermal status that `PowerManager` of Android reports from Android 10 on. It compiles on every platform, so the tests of every host check it.
class AndroidThermalStatus final {
  public:
    // Takes a value of `PowerManager.THERMAL_STATUS_*`. Light and moderate throttling, which the person barely notices, count as fair, severe throttling as serious, and the statuses from critical to shutdown as critical. A value that Android does not define is unknown.
    [[nodiscard]] static ThermalState toThermalState(int status) noexcept;

  private:
    // The values of `PowerManager.THERMAL_STATUS_*`.
    static constexpr int kNone = 0;
    static constexpr int kLight = 1;
    static constexpr int kModerate = 2;
    static constexpr int kSevere = 3;
    static constexpr int kCritical = 4;
    static constexpr int kEmergency = 5;
    static constexpr int kShutdown = 6;
};

} // namespace haylen::platform
