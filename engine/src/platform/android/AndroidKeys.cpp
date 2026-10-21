#include "platform/android/AndroidKeys.hpp"

#include "platform/Player.hpp"

namespace haylen::platform {

void AndroidKeys::receiveBack() {
    Player::postBack();
}

} // namespace haylen::platform
