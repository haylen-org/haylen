#import "platform/apple/ApplePermissions.hpp"

#include <TargetConditionals.h>

#include <utility>

#include "platform/NativeWindow.hpp"
#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
#import "platform/apple/AppleOutputs.hpp"
#endif

namespace haylen::platform {

void ApplePermissions::check(Permission::Kind kind, std::function<void(Permission::Answer answer)> callback) {
    switch (kind) {
    case Permission::Kind::Screens:
        callback({.state = Permission::State::Granted});
        return;
    case Permission::Kind::LocalNetwork:
        // Apple platforms ask the person for the local network the first time the app reaches it, and tell apps neither the answer nor a way to ask before.
        callback({.state = Permission::State::Unknown});
        return;
    }
}

void ApplePermissions::request(Permission::Kind kind, std::function<void(Permission::Answer answer)> callback) {
    switch (kind) {
    case Permission::Kind::Screens:
        // An iPhone or an iPad of iOS 27 and later mirrors an external display until the app registers the scene accessory that connects the scene of that display, which needs no answer of the person.
#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
        if (UIWindow* window = (__bridge UIWindow*)NativeWindow::getHandle(); window.rootViewController != nil) {
            AppleOutputs::registerAccessory(window.rootViewController);
        }
#endif
        check(kind, std::move(callback));
        return;
    case Permission::Kind::LocalNetwork:
        check(kind, std::move(callback));
        return;
    }
}

} // namespace haylen::platform
