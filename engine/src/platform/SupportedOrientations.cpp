#include "platform/SupportedOrientations.hpp"

namespace haylen::platform {

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
