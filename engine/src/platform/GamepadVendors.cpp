#include "platform/GamepadVendors.hpp"

#include <algorithm>

namespace haylen::platform {

input::GamepadLabels GamepadVendors::getLabels(std::uint16_t vendor) noexcept {
    const auto found = std::ranges::find(kVendors, vendor, &std::pair<std::uint16_t, input::GamepadLabels>::first);
    return found == kVendors.end() ? input::GamepadLabels::Unknown : found->second;
}

} // namespace haylen::platform
