#include "platform/android/AndroidKeyClaims.hpp"

#include <cstddef>
#include <utility>

namespace haylen::platform {

bool AndroidKeyClaims::take(std::int32_t code, Action action, bool wanted) noexcept {
    if (code < 0 || code >= kKeyCodes) {
        return false;
    }

    bool& claim = taken[static_cast<std::size_t>(code)];
    switch (action) {
    case Action::Press:
        claim = wanted;
        return claim;
    case Action::Repeat:
        return claim;
    case Action::Release:
        return std::exchange(claim, false);
    }
    return false;
}

} // namespace haylen::platform
