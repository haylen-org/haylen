#include "platform/android/AndroidThermalStatus.hpp"

namespace haylen::platform {

ThermalState AndroidThermalStatus::toThermalState(int status) noexcept {
    switch (status) {
    case kNone:
        return ThermalState::Nominal;
    case kLight:
    case kModerate:
        return ThermalState::Fair;
    case kSevere:
        return ThermalState::Serious;
    case kCritical:
    case kEmergency:
    case kShutdown:
        return ThermalState::Critical;
    default:
        return ThermalState::Unknown;
    }
}

} // namespace haylen::platform
