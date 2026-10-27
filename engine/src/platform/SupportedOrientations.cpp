#include "platform/SupportedOrientations.hpp"

namespace haylen::platform {

std::uint32_t SupportedOrientations::fromInfoPlist(std::span<const std::string_view> names) noexcept {
    std::uint32_t mask = 0;
    for (const std::string_view name : names) {
        for (const auto& [known, bit] : kNames) {
            mask |= name == known ? bit : 0U;
        }
    }
    return mask != 0 ? mask : kAll;
}

std::uint32_t SupportedOrientations::fromLock(Orientation value, bool pad) noexcept {
    const std::uint32_t portrait = pad ? kPortrait | kPortraitUpsideDown : kPortrait;
    switch (value) {
    case Orientation::Landscape:
        return kLandscape;
    case Orientation::Portrait:
        return portrait;
    case Orientation::Any:
        return portrait | kLandscape;
    }
    return kAll;
}

} // namespace haylen::platform
