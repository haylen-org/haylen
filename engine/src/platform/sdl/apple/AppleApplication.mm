#import "platform/sdl/apple/AppleApplication.hpp"

#include "platform/NativeWindow.hpp"
#include "platform/sdl/Runtime.hpp"

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>

#import "platform/apple/AppleScroll.hpp"
#import "platform/apple/HaylenFieldEditor.h"
#import "platform/sdl/apple/HaylenAppDelegate.h"
#else
#import <UIKit/UIKit.h>
#include <objc/runtime.h>

#include <SDL3/SDL_hints.h>

#import "platform/apple/AppleSplash.hpp"
#import "platform/apple/AppleTheme.hpp"
#import "platform/apple/HaylenAccessibilityView.h"
#import "platform/apple/HaylenOverlayLayer.h"
#import "platform/sdl/apple/HaylenRootViewController.h"
#import "platform/sdl/apple/HaylenSceneDelegate.h"
#endif

namespace haylen::platform::sdl {

#if TARGET_OS_OSX
// The application keeps its delegate for the whole process, since AppKit holds it only weakly.
void AppleApplication::prepare() {
    static HaylenAppDelegate* delegate = [HaylenAppDelegate new];
    [NSApplication sharedApplication];
    NSApp.activationPolicy = NSApplicationActivationPolicyRegular;
    NSApp.delegate = delegate;
}

// The scrolls of the window reach the app in the order of the events that SDL reads, with the phases of the trackpad that SDL leaves out.
void AppleApplication::finishLaunching(Runtime& runtime) {
    [NSApp finishLaunching];
    AppleScroll::watch([&runtime](const Event& event) { runtime.post(event); });
}

bool AppleApplication::isTextFieldFocused() noexcept {
    NSWindow* window = (__bridge NSWindow*)NativeWindow::getHandle();
    return [window.firstResponder isKindOfClass:HaylenFieldEditor.class];
}
#else
// SDL names the class that UIKit creates as the delegate of the application through a class method of its own delegate, which names the delegate of Haylen instead. SDL takes every orientation, so it never turns the size of the window it opens, and once the window opened its view controller and the application delegate answer UIKit with the orientations of the `Info.plist` or of the lock of the app.
void AppleApplication::prepare() {
    const IMP name = imp_implementationWithBlock(^NSString*(id) { return NSStringFromClass(HaylenSceneDelegate.class); });
    class_replaceMethod(object_getClass(HaylenSceneDelegate.superclass), @selector(getSceneDelegateClassName), name, "@@:");
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight Portrait PortraitUpsideDown");
}

// The window that SDL opened lies under the overlay of the plugins and the elements of screen readers, and under the launch screen until the splash ends. The view controller of SDL answers UIKit with the status bar and the orientations of the app, and the plugins hear the scene connect once the window of the app is in it.
void AppleApplication::finishLaunching(Runtime&) {
    UIWindow* window = (__bridge UIWindow*)NativeWindow::getHandle();
#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
    object_setClass(window.rootViewController, HaylenRootViewController.class);
#endif
    UIView* content = window.rootViewController.view;
    [HaylenAccessibilityView.shared attachToView:content];
    [HaylenOverlayLayer.shared attachToView:content];
    AppleSplash::cover(window);
    AppleTheme::observe(window);
    [static_cast<HaylenSceneDelegate*>(window.windowScene.delegate) connectPlugins];
}
#endif

} // namespace haylen::platform::sdl
