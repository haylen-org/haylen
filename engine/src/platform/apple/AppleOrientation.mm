#import "platform/apple/AppleOrientation.hpp"

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
#import <objc/runtime.h>

#include "sokol_app.h"

namespace haylen::platform {

UIInterfaceOrientationMask AppleOrientation::mask = 0;

Orientation AppleOrientation::get() {
    UIWindow* window = (__bridge UIWindow*)sapp_ios_get_window();
    const UIInterfaceOrientation orientation = window.windowScene.interfaceOrientation;
    if (orientation == UIInterfaceOrientationUnknown) {
        return sapp_height() > sapp_width() ? Orientation::Portrait : Orientation::Landscape;
    }
    return UIInterfaceOrientationIsPortrait(orientation) ? Orientation::Portrait : Orientation::Landscape;
}

UIInterfaceOrientationMask AppleOrientation::toMask(Orientation value) {
    const bool pad = UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPad;
    switch (value) {
    case Orientation::Landscape:
        return UIInterfaceOrientationMaskLandscape;
    case Orientation::Portrait:
        return pad ? UIInterfaceOrientationMaskPortrait | UIInterfaceOrientationMaskPortraitUpsideDown : UIInterfaceOrientationMaskPortrait;
    case Orientation::Any:
        return pad ? UIInterfaceOrientationMaskAll : UIInterfaceOrientationMaskAllButUpsideDown;
    }
    return UIInterfaceOrientationMaskAll;
}

// UIKit asks the application delegate for the allowed orientations whenever it rotates, so the lock holds after the geometry update turns the screen. The delegate of sokol_app does not answer that question, so the first lock gives it the answer.
void AppleOrientation::lock(Orientation value) {
    const bool installed = mask != 0;
    mask = toMask(value);
    if (!installed) {
        IMP answer = imp_implementationWithBlock(^UIInterfaceOrientationMask(id, UIApplication*, UIWindow*) { return mask; });
        class_addMethod(object_getClass(UIApplication.sharedApplication.delegate), @selector(application:supportedInterfaceOrientationsForWindow:), answer, "Q@:@@");
    }

    UIWindow* window = (__bridge UIWindow*)sapp_ios_get_window();
    [window.rootViewController setNeedsUpdateOfSupportedInterfaceOrientations];
    UIWindowSceneGeometryPreferencesIOS* preferences = [[UIWindowSceneGeometryPreferencesIOS alloc] initWithInterfaceOrientations:mask];
    [window.windowScene requestGeometryUpdateWithPreferences:preferences errorHandler:nil];
}

} // namespace haylen::platform
#endif
