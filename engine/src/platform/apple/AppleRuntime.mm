#include "platform/apple/AppleRuntime.hpp"

#import "platform/apple/AppleBridge.hpp"

namespace haylen::platform {

void AppleRuntime::setAppRunning(bool value) {
    AppleBridge::setAppRunning(value);
}

} // namespace haylen::platform
