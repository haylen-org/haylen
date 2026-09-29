#include "platform/apple/AppleRuntime.hpp"

#include <TargetConditionals.h>
#include <objc/runtime.h>

#import "platform/apple/AppleBridge.hpp"

#if TARGET_OS_OSX
#import "platform/apple/HaylenAppDelegate.h"
#else
#import "platform/apple/HaylenSceneDelegate.h"
#endif

namespace haylen::platform {

// Naming the class through the class itself links its implementation, which nothing else refers to, into apps that link the engine as a static library.
const char* AppleRuntime::getDelegateClass() noexcept {
#if TARGET_OS_OSX
    return class_getName([HaylenAppDelegate class]);
#else
    return class_getName([HaylenSceneDelegate class]);
#endif
}

void AppleRuntime::setAppRunning(bool value) {
    AppleBridge::setAppRunning(value);
}

} // namespace haylen::platform
