#import "platform/apple/HaylenScreen+Runtime.h"

#include <optional>
#include <utility>

#include "haylen/core/Log.hpp"
#include "platform/ScreenRelay.hpp"
#import "platform/apple/AppleBridge.hpp"
#import "platform/apple/AppleScreens.hpp"
#include "sokol_app.h"

#if !TARGET_OS_OSX
#import "platform/apple/ApplePresenter.hpp"
#import "platform/apple/HaylenScreenController.h"
#endif

using haylen::core::Log;
using haylen::platform::AppleBridge;
using haylen::platform::AppleScreens;
using haylen::platform::ScreenRelay;

@implementation HaylenScreen {
    std::uint64_t screenId;
    NSString* plugin;
    NSString* screenName;
    BOOL screenOpaque;

    // The UI that shows the screen, `nil` while none shows: the container of a presented controller, the activity that asks for a window and then the scene of the window, or the sheet or the window on macOS.
    id shown;

    // Whether the container waits for a running transition before it presents, which relays an end that arrives meanwhile instead.
    BOOL waiting;

    // The end of the screen once it has one, and whether it reached the engine.
    std::optional<AppleBridge::Encoded> ending;
    BOOL succeeded;
    BOOL relayed;
#if TARGET_OS_OSX
    BOOL inSheet;
    id closing;
#elif !TARGET_OS_TV
    UIViewController* windowContent;
#endif
}

- (instancetype)initWithRequest:(const haylen::platform::ScreenRequest&)request {
    self = [super init];
    screenId = request.id;
    plugin = @(request.plugin.c_str());
    screenName = @(request.screen.c_str());
    screenOpaque = request.opaque;
    return self;
}

- (NSString*)name {
    return screenName;
}

- (BOOL)isOpaque {
    return screenOpaque;
}

- (std::uint64_t)identifier {
    return screenId;
}

- (BOOL)isEnded {
    return ending.has_value();
}

- (void)finishWithResult:(id)result {
    if (!NSThread.isMainThread) {
        dispatch_async(dispatch_get_main_queue(), ^{ [self finishWithResult:result]; });
        return;
    }
    std::optional<AppleBridge::Encoded> encoded = AppleBridge::encode(result);
    if (!encoded) {
        [self failWithMessage:[NSString stringWithFormat:@"The screen \"%@.%@\" ended with a result that is not JSON.", plugin, screenName] code:nil data:nil];
        return;
    }
    [self end:std::move(*encoded) succeeded:YES];
}

// A failure whose data JSON cannot hold ends with its message and code alone.
- (void)failWithMessage:(NSString*)message code:(NSString*)code data:(id)data {
    if (!NSThread.isMainThread) {
        dispatch_async(dispatch_get_main_queue(), ^{ [self failWithMessage:message code:code data:data]; });
        return;
    }
    NSMutableDictionary* failure = [NSMutableDictionary dictionaryWithObject:message forKey:@"message"];
    failure[@"code"] = code;
    failure[@"data"] = data;
    std::optional<AppleBridge::Encoded> encoded = AppleBridge::encode(failure);
    if (!encoded) {
        [failure removeObjectForKey:@"data"];
        encoded = AppleBridge::encode(failure);
    }
    [self end:std::move(*encoded) succeeded:NO];
}

// The first end counts. The UI that shows goes away first, and the end reaches the engine once it is gone.
- (void)end:(AppleBridge::Encoded)encoded succeeded:(BOOL)ok {
    if (ending) {
        return;
    }
    ending = std::move(encoded);
    succeeded = ok;
    if (shown == nil) {
        [self relay];
    } else if (!waiting) {
        [self dismissShown];
    }
}

- (void)cancel {
    void (^handler)(void) = self.cancelHandler;
    self.cancelHandler = nil;
    if (shown != nil || handler == nil) {
        [self endWithCode:@"cancelled" message:@"The app gave the screen up."];
    }
    if (handler != nil) {
        handler();
    }
}

- (void)dismissed {
#if TARGET_OS_OSX
    if (closing != nil) {
        [NSNotificationCenter.defaultCenter removeObserver:closing];
        closing = nil;
    }
#endif
    shown = nil;
    if (!ending) {
        [self endWithCode:@"cancelled" message:[NSString stringWithFormat:@"The person closed the screen \"%@.%@\".", plugin, screenName]];
        return;
    }
    [self relay];
}

- (void)endWithCode:(NSString*)code message:(NSString*)message {
    [self end:*AppleBridge::encode(@{@"message" : message, @"code" : code}) succeeded:NO];
}

- (void)relay {
    if (relayed) {
        return;
    }
    relayed = YES;
    ScreenRelay::finish(screenId, succeeded == YES, ending->json, std::move(ending->buffers));
    AppleScreens::forget(screenId);
}

// A screen shows its UI once, and UI that arrives after the screen ended never shows.
- (BOOL)canShow {
    if (ending || shown != nil) {
        Log::error("The screen \"{}.{}\" shows its UI once, before it ends.", plugin.UTF8String, screenName.UTF8String);
        return NO;
    }
    return YES;
}

#if TARGET_OS_OSX
- (NSWindow*)window {
    return sapp_isvalid() ? (__bridge NSWindow*)sapp_macos_get_window() : nil;
}

- (void)presentSheet:(NSViewController*)controller {
    NSWindow* parent = self.window;
    if (![self canShow] || ![self hasWindow:parent]) {
        return;
    }
    NSWindow* sheet = [NSWindow windowWithContentViewController:controller];
    shown = sheet;
    inSheet = YES;
    [self followClose:sheet];
    [parent beginSheet:sheet completionHandler:^(NSModalResponse) { [self dismissed]; }];
}

// The window stays above the window of the app at its level, so it shows over apps that float above other windows too.
- (void)presentWindow:(NSViewController*)controller {
    NSWindow* parent = self.window;
    if (![self canShow] || ![self hasWindow:parent]) {
        return;
    }
    NSWindow* window = [NSWindow windowWithContentViewController:controller];
    window.releasedWhenClosed = NO;
    window.level = parent.level;
    const NSSize size = window.frame.size;
    [window setFrameOrigin:NSMakePoint(NSMidX(parent.frame) - (size.width / 2.0), NSMidY(parent.frame) - (size.height / 2.0))];
    shown = window;
    inSheet = NO;
    [self followClose:window];
    [parent addChildWindow:window ordered:NSWindowAbove];
    [window makeKeyAndOrderFront:nil];
}

- (BOOL)hasWindow:(NSWindow*)parent {
    if (parent == nil) {
        [self endWithCode:@"notActive" message:@"The app has no window to show the screen on."];
        return NO;
    }
    return YES;
}

// A sheet that closes without ending, as SwiftUI closes its window, ends first, and the completion of the sheet reports it.
- (void)followClose:(NSWindow*)window {
    // clang-format off
    closing = [NSNotificationCenter.defaultCenter addObserverForName:NSWindowWillCloseNotification object:window queue:nil usingBlock:^(NSNotification*) {
        if (self->inSheet) {
            [window.sheetParent endSheet:window];
            return;
        }
        [window.parentWindow removeChildWindow:window];
        [self dismissed];
    }];
    // clang-format on
}

- (void)dismissShown {
    NSWindow* window = shown;
    if (inSheet) {
        [self.window endSheet:window returnCode:NSModalResponseCancel];
        return;
    }
    [window close];
}
#else
- (UIViewController*)presenter {
    return haylen::platform::ApplePresenter::getTopmost();
}

// A controller that keeps the automatic presentation style presents full screen for an opaque screen and as a sheet otherwise.
- (void)presentViewController:(UIViewController*)controller {
    if (![self canShow]) {
        return;
    }
    UIModalPresentationStyle style = controller.modalPresentationStyle;
    if (style == UIModalPresentationAutomatic && screenOpaque) {
        style = UIModalPresentationFullScreen;
    }
    shown = [[HaylenScreenController alloc] initWithScreen:self content:controller style:style];
    [self presentContainer];
}

- (void)presentContainer {
    waiting = NO;
    if (ending) {
        shown = nil;
        [self relay];
        return;
    }
    UIViewController* top = haylen::platform::ApplePresenter::getTopmost();
    if (top == nil) {
        shown = nil;
        [self endWithCode:@"notActive" message:@"The app has no window to show the screen over."];
        return;
    }
    id<UIViewControllerTransitionCoordinator> coordinator = top.transitionCoordinator;
    if (coordinator != nil) {
        waiting = YES;
        [coordinator animateAlongsideTransition:nil completion:^(id<UIViewControllerTransitionCoordinatorContext>) { [self presentContainer]; }];
        return;
    }
    [top presentViewController:shown animated:YES completion:nil];
}

#if !TARGET_OS_TV
- (void)presentWindow:(UIViewController*)controller {
    if (![self canShow]) {
        return;
    }
    if (!UIApplication.sharedApplication.supportsMultipleScenes) {
        [self endWithCode:@"unsupported" message:@"This device shows the app in one window, so a screen opens no window of its own."];
        return;
    }
    windowContent = controller;
    NSUserActivity* activity = [[NSUserActivity alloc] initWithActivityType:AppleScreens::kWindowActivity];
    activity.userInfo = @{@"screen" : @(screenId)};
    shown = activity;
    // clang-format off
    [UIApplication.sharedApplication requestSceneSessionActivation:nil userActivity:activity options:nil errorHandler:^(NSError* error) {
        dispatch_async(dispatch_get_main_queue(), ^{
            if (self->shown == activity) {
                self->shown = nil;
                [self endWithCode:@"failed" message:[NSString stringWithFormat:@"The window of the screen could not open. %@", error.localizedDescription]];
            }
        });
    }];
    // clang-format on
}

// Mac Catalyst opens the window at the size the controller prefers, and every other platform at the size the system picks.
- (UIWindow*)windowForScene:(UIWindowScene*)scene {
    UIWindow* window = [[UIWindow alloc] initWithWindowScene:scene];
    window.rootViewController = windowContent;
    scene.title = windowContent.title;
#if TARGET_OS_MACCATALYST
    const CGSize size = windowContent.preferredContentSize;
    if (size.width > 0.0 && size.height > 0.0) {
        const CGRect screen = scene.screen.bounds;
        const CGRect frame = CGRectMake(CGRectGetMidX(screen) - (size.width / 2.0), CGRectGetMidY(screen) - (size.height / 2.0), size.width, size.height);
        [scene requestGeometryUpdateWithPreferences:[[UIWindowSceneGeometryPreferencesMac alloc] initWithSystemFrame:frame] errorHandler:nil];
    }
#endif
    shown = scene;
    return window;
}
#endif

- (void)dismissShown {
    if ([shown isKindOfClass:UIViewController.class]) {
        UIViewController* container = shown;
        if (container.presentingViewController == nil) {
            [self dismissed];
            return;
        }
        [container.presentingViewController dismissViewControllerAnimated:YES completion:^{ [self dismissed]; }];
        return;
    }
#if !TARGET_OS_TV
    if ([shown isKindOfClass:UIWindowScene.class]) {
        [UIApplication.sharedApplication requestSceneSessionDestruction:static_cast<UIWindowScene*>(shown).session options:nil errorHandler:nil];
        return;
    }
#endif
    [self dismissed];
}
#endif

@end
