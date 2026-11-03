#include "platform/android/AndroidKeys.hpp"

#include "platform/Player.hpp"

namespace haylen::platform {

void AndroidKeys::receiveBack() {
    Player::postBack();
}

bool AndroidKeys::receiveBackHeld(bool pressed) {
    return Player::holdBack(pressed);
}

} // namespace haylen::platform
