#import "platform/apple/AppleOrientation.hpp"

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
#include <string_view>
#include <vector>

#include "haylen/core/Log.hpp"
#include "platform/NativeWindow.hpp"

namespace haylen::platform {

Orientation AppleOrientation::get() {
    UIWindow* window = (__bridge UIWindow*)NativeWindow::getHandle();
    const UIInterfaceOrientation orientation = window.windowScene.interfaceOrientation;
    if (orientation == UIInterfaceOrientationUnknown) {
        const math::Vec2 size = NativeWindow::getFramebufferSize();
        return size.y > size.x ? Orientation::Portrait : Orientation::Landscape;
    }
    return UIInterfaceOrientationIsPortrait(orientation) ? Orientation::Portrait : Orientation::Landscape;
}

void AppleOrientation::lock(Orientation value) {
    getOrientations().lock(value, UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPad);
    const UIInterfaceOrientationMask mask = getOrientations().get();
    UIWindow* window = (__bridge UIWindow*)NativeWindow::getHandle();
    [window.rootViewController setNeedsUpdateOfSupportedInterfaceOrientations];
    UIWindowSceneGeometryPreferencesIOS* preferences = [[UIWindowSceneGeometryPreferencesIOS alloc] initWithInterfaceOrientations:mask];
    [window.windowScene requestGeometryUpdateWithPreferences:preferences errorHandler:^(NSError* error) { core::Log::warning("The screen did not turn to the locked orientation. {}", error.localizedDescription.UTF8String); }];
}

UIInterfaceOrientationMask AppleOrientation::getSupported() {
    return getOrientations().get();
}

SupportedOrientations& AppleOrientation::getOrientations() {
    static SupportedOrientations orientations = readInfoPlist();
    return orientations;
}

// The keys of iPads come first on an iPad, as UIKit reads them.
SupportedOrientations AppleOrientation::readInfoPlist() {
    NSDictionary* info = NSBundle.mainBundle.infoDictionary;
    const bool pad = UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPad;
    NSArray<NSString*>* names = (pad ? info[@"UISupportedInterfaceOrientations~ipad"] : nil) ?: info[@"UISupportedInterfaceOrientations"];
    std::vector<std::string_view> listed;
    for (NSString* name in names) {
        listed.emplace_back(name.UTF8String);
    }
    SupportedOrientations orientations;
    orientations.setListed(SupportedOrientations::fromInfoPlist(listed));
    return orientations;
}

static_assert(SupportedOrientations::kPortrait == UIInterfaceOrientationMaskPortrait && SupportedOrientations::kPortraitUpsideDown == UIInterfaceOrientationMaskPortraitUpsideDown && SupportedOrientations::kLandscapeLeft == UIInterfaceOrientationMaskLandscapeLeft && SupportedOrientations::kLandscapeRight == UIInterfaceOrientationMaskLandscapeRight);

} // namespace haylen::platform
#endif
