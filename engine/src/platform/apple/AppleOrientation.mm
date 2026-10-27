#import "platform/apple/AppleOrientation.hpp"

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
#include "haylen/core/Log.hpp"
#include "platform/NativeWindow.hpp"

namespace haylen::platform {

UIInterfaceOrientationMask AppleOrientation::mask = 0;

Orientation AppleOrientation::get() {
    UIWindow* window = (__bridge UIWindow*)NativeWindow::getHandle();
    const UIInterfaceOrientation orientation = window.windowScene.interfaceOrientation;
    if (orientation == UIInterfaceOrientationUnknown) {
        const math::Vec2 size = NativeWindow::getFramebufferSize();
        return size.y > size.x ? Orientation::Portrait : Orientation::Landscape;
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

void AppleOrientation::lock(Orientation value) {
    mask = toMask(value);
    UIWindow* window = (__bridge UIWindow*)NativeWindow::getHandle();
    [window.rootViewController setNeedsUpdateOfSupportedInterfaceOrientations];
    UIWindowSceneGeometryPreferencesIOS* preferences = [[UIWindowSceneGeometryPreferencesIOS alloc] initWithInterfaceOrientations:mask];
    [window.windowScene requestGeometryUpdateWithPreferences:preferences errorHandler:^(NSError* error) { core::Log::warning("The screen did not turn to the locked orientation. {}", error.localizedDescription.UTF8String); }];
}

UIInterfaceOrientationMask AppleOrientation::getSupported() {
    return mask != 0 ? mask : readInfoPlist();
}

// The keys of iPads come first on an iPad, as UIKit reads them.
UIInterfaceOrientationMask AppleOrientation::readInfoPlist() {
    NSDictionary* info = NSBundle.mainBundle.infoDictionary;
    const bool pad = UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPad;
    NSArray<NSString*>* names = (pad ? info[@"UISupportedInterfaceOrientations~ipad"] : nil) ?: info[@"UISupportedInterfaceOrientations"];
    NSDictionary<NSString*, NSNumber*>* masks = @{
        @"UIInterfaceOrientationPortrait" : @(UIInterfaceOrientationMaskPortrait),
        @"UIInterfaceOrientationPortraitUpsideDown" : @(UIInterfaceOrientationMaskPortraitUpsideDown),
        @"UIInterfaceOrientationLandscapeLeft" : @(UIInterfaceOrientationMaskLandscapeLeft),
        @"UIInterfaceOrientationLandscapeRight" : @(UIInterfaceOrientationMaskLandscapeRight),
    };
    UIInterfaceOrientationMask supported = 0;
    for (NSString* name in names) {
        supported |= masks[name].unsignedIntegerValue;
    }
    return supported != 0 ? supported : UIInterfaceOrientationMaskAll;
}

} // namespace haylen::platform
#endif
