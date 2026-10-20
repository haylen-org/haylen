#include "platform/android/AndroidKeys.hpp"

#include "platform/sokol/SokolRuntime.hpp"

namespace haylen::platform {

void AndroidKeys::receiveBack() {
    SokolRuntime::postBack();
}

} // namespace haylen::platform
