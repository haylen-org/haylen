#import "platform/apple/AppleWindowSize.hpp"

#if TARGET_OS_VISION
#import <UIKit/UIKit.h>

#include "haylen/core/Log.hpp"
#include "platform/NativeWindow.hpp"

namespace haylen::platform {

// The system keeps the window within the sizes it allows, and a size that it refuses leaves the window as it was, which the log tells.
void AppleWindowSize::requestSize(math::Vec2 points) {
    UIWindow* window = (__bridge UIWindow*)NativeWindow::getHandle();
    UIWindowScene* scene = window.windowScene;
    if (scene == nil) {
        return;
    }

    UIWindowSceneGeometryPreferencesVision* preferences = [[UIWindowSceneGeometryPreferencesVision alloc] initWithSize:CGSizeMake(points.x, points.y)];
    [scene requestGeometryUpdateWithPreferences:preferences errorHandler:^(NSError* error) { core::Log::warning("The window did not take the size that the app asked for. {}", error.localizedDescription.UTF8String); }];
}

} // namespace haylen::platform
#endif
