#include "platform/windows/WindowsPowerStatus.hpp"

namespace haylen::platform {

// A battery on mains power that does not charge, as one that is full or held below full by the power settings, counts as full, as on the other desktops.
Battery WindowsPowerStatus::toBattery(std::uint8_t lineStatus, std::uint8_t flags, std::uint8_t percent) {
    if (flags == kUnknown) {
        return {};
    }
    if ((flags & kNoBattery) != 0) {
        return {.state = Battery::State::None};
    }

    Battery battery;
    if (percent <= 100) {
        battery.level = static_cast<float>(percent) / 100.0F;
    }
    if ((flags & kCharging) != 0) {
        battery.charging = true;
        battery.state = Battery::State::Charging;
    } else if (lineStatus == kOnline) {
        battery.state = Battery::State::Full;
    } else if (lineStatus == kOffline) {
        battery.state = Battery::State::Discharging;
    }
    return battery;
}

} // namespace haylen::platform
