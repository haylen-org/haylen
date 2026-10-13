#include "platform/GamepadVendors.hpp"

#include <algorithm>
#include <charconv>

namespace haylen::platform {

input::GamepadLabels GamepadVendors::getLabels(std::uint16_t vendor) noexcept {
    const auto found = std::ranges::find(kVendors, vendor, &std::pair<std::uint16_t, input::GamepadLabels>::first);
    return found == kVendors.end() ? input::GamepadLabels::Unknown : found->second;
}

std::optional<std::uint16_t> GamepadVendors::parseHex(std::string_view digits) noexcept {
    if (digits.empty() || digits.size() > 4) {
        return std::nullopt;
    }
    std::uint16_t value = 0;
    const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), value, 16);
    return error == std::errc() && end == digits.data() + digits.size() ? std::optional(value) : std::nullopt;
}

std::optional<std::uint16_t> GamepadVendors::findInBrowserId(std::string_view id) noexcept {
    constexpr std::string_view kMarker = "Vendor: ";
    if (const std::size_t marker = id.find(kMarker); marker != std::string_view::npos) {
        const std::string_view rest = id.substr(marker + kMarker.size());
        return parseHex(rest.substr(0, rest.find_first_not_of("0123456789abcdefABCDEF")));
    }
    const std::size_t dash = id.find('-');
    const std::size_t second = dash == std::string_view::npos ? dash : id.find('-', dash + 1);
    if (second == std::string_view::npos || !parseHex(id.substr(dash + 1, second - dash - 1))) {
        return std::nullopt;
    }
    return parseHex(id.substr(0, dash));
}

} // namespace haylen::platform
