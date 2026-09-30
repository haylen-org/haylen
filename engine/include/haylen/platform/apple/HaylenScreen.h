#pragma once

#include <TargetConditionals.h>

#import <Foundation/Foundation.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif

NS_ASSUME_NONNULL_BEGIN

@class HaylenScreen;

// Opens a screen. The runtime calls it on the main queue once the engine covered the app, with the parameters of the app, where bytes arrive as `NSData` values, and the screen, through which the handler shows its UI and which it ends.
typedef void (^HaylenScreenHandler)(id params, HaylenScreen* screen);

// A screen of a plugin: native UI that takes over the app until it ends with one result, such as a paywall, a sign-in flow or the UI of an SDK. The handler shows its UI through the screen and ends the screen with `finish` or `fail` from any thread, after which the runtime dismisses the UI and hands the end to the app once the UI is gone. UI that goes away before the screen ended, because the person closed it or because the UI, SwiftUI or an SDK dismissed it, ends the screen with the code `cancelled`. The first end counts.
NS_SWIFT_UI_ACTOR
@interface HaylenScreen : NSObject

@property(nonatomic, readonly, copy) NSString* name;

// Whether the app asked for a screen that hides it completely. A controller whose presentation style is automatic presents full screen for an opaque screen, and as a sheet over the app otherwise.
@property(nonatomic, readonly, getter=isOpaque) BOOL opaque;

// Runs once when the app gives the screen up, through a cancel or a timeout, after the runtime began to dismiss the UI it shows, so a screen whose UI an SDK shows by itself closes it and ends. A screen that shows nothing through the runtime and has no cancel handler ends with the code `cancelled` at once.
@property(nonatomic, copy, nullable) void (^cancelHandler)(void);

#if TARGET_OS_OSX
// The window of the app, which owns the sheets and the windows of screens.
@property(nonatomic, readonly, nullable) NSWindow* window;

// Shows the controller in a sheet of the window of the app, which keeps the events of the window away while it shows.
- (void)presentSheet:(NSViewController*)controller NS_SWIFT_NAME(presentSheet(_:));

// Shows the controller in a window of its own, a child of the window of the app that moves with it, which suits borderless and transparent app windows. Its close button ends the screen with the code `cancelled`.
- (void)presentWindow:(NSViewController*)controller NS_SWIFT_NAME(presentWindow(_:));
#else
// The topmost view controller that the window of the app presents, for SDKs that present their UI themselves.
@property(nonatomic, readonly, nullable) UIViewController* presenter;

// Presents the controller from the topmost presented controller, once a running transition ended, inside a container that sees every way the controller goes away: the end of the screen, the swipe of the person, and the dismissal by the controller, by SwiftUI or by an SDK. SwiftUI views present through a `UIHostingController`.
- (void)presentViewController:(UIViewController*)controller NS_SWIFT_NAME(present(_:));

// Shows the controller in a window of its own on Mac Catalyst and on iPads that show several windows of an app, and fails the screen with the code `unsupported` on other devices. Closing the window ends the screen with the code `cancelled`.
- (void)presentWindow:(UIViewController*)controller NS_SWIFT_NAME(presentWindow(_:)) API_UNAVAILABLE(tvos);
#endif

// Ends the screen with a result, any value `NSJSONSerialization` accepts, with `NSData` values anywhere inside that cross as bytes, or `nil`.
- (void)finishWithResult:(nullable id)result NS_SWIFT_NAME(finish(_:));

// Ends the screen with a failure, whose code and data the call of the app keeps, like the failure of a platform call.
- (void)failWithMessage:(NSString*)message code:(nullable NSString*)code data:(nullable id)data NS_SWIFT_NAME(fail(_:code:data:));

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
