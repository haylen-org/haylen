#include "platform/android/AndroidBatteryStatus.hpp"

#include <algorithm>

namespace haylen::platform {

// A device without a battery, such as a TV, reports none. A plugged battery that Android keeps from charging, such as to protect it, counts as discharging, and a full one does not charge, as on the other platforms.
Battery AndroidBatteryStatus::toBattery(bool present, int level, int scale, int status) {
    if (!present) {
        return {.state = Battery::State::None};
    }

    Battery battery;
    if (level >= 0 && scale > 0) {
        battery.level = std::clamp(static_cast<float>(level) / static_cast<float>(scale), 0.0F, 1.0F);
    }
    if (status == kCharging) {
        battery.charging = true;
        battery.state = Battery::State::Charging;
    } else if (status == kDischarging || status == kNotCharging) {
        battery.state = Battery::State::Discharging;
    } else if (status == kFull) {
        battery.state = Battery::State::Full;
    }
    return battery;
}

} // namespace haylen::platform
