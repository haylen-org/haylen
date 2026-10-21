#include "backends/sokol/apple/AppleClasses.hpp"

#include <TargetConditionals.h>
#include <objc/runtime.h>

#if TARGET_OS_OSX
#import "backends/sokol/apple/HaylenAppDelegate.h"
#else
#import "backends/sokol/apple/HaylenSceneDelegate.h"
#import "platform/apple/HaylenViewController.h"
#endif

namespace haylen::sokol {

// Naming the class through the class itself links its implementation, which nothing else refers to, into apps that link the engine as a static library.
const char* AppleClasses::getDelegateClass() noexcept {
#if TARGET_OS_OSX
    return class_getName([HaylenAppDelegate class]);
#else
    return class_getName([HaylenSceneDelegate class]);
#endif
}

// Only iPhones and iPads show a status bar, which the view controller of the window answers UIKit about.
const char* AppleClasses::getViewControllerClass() noexcept {
#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
    return class_getName([HaylenViewController class]);
#else
    return nullptr;
#endif
}

} // namespace haylen::sokol
