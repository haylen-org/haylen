#pragma once

#include <TargetConditionals.h>

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif

#import "haylen/platform/apple/HaylenAudioStream.h"
#import "haylen/platform/apple/HaylenBridge.h"
#import "haylen/platform/apple/HaylenScreen.h"
#import "haylen/platform/apple/HaylenVideoStream.h"

NS_ASSUME_NONNULL_BEGIN

@class HaylenOverlay;
@class HaylenOverlayItem;
@class HaylenPlacement;

typedef NS_ENUM(NSInteger, HaylenPlacementAnchor) {
    HaylenPlacementAnchorTop,
    HaylenPlacementAnchorBottom,
    HaylenPlacementAnchorLeft,
    HaylenPlacementAnchorRight,
    HaylenPlacementAnchorTopLeft,
    HaylenPlacementAnchorTopRight,
    HaylenPlacementAnchorBottomLeft,
    HaylenPlacementAnchorBottomRight,
    HaylenPlacementAnchorCenter,
} NS_SWIFT_NAME(HaylenPlacement.Anchor);

// Where the overlay places a view: at its anchor, margin points away from the edges the anchor names, inside the safe area unless insideSafeArea is NO, and with the size of the view unless width or height are above zero. A view whose placement reserves takes the edge its anchor names while it is visible, so the UI of the app moves out of its way. A centered view reserves nothing. A new placement sits at the bottom inside the safe area and reserves nothing.
@interface HaylenPlacement : NSObject <NSCopying>

@property(nonatomic) HaylenPlacementAnchor anchor;
@property(nonatomic) CGFloat margin;
@property(nonatomic) BOOL insideSafeArea;
@property(nonatomic) BOOL reserve;
@property(nonatomic) CGFloat width;
@property(nonatomic) CGFloat height;

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithAnchor:(HaylenPlacementAnchor)anchor;

@end

// Places native views of a plugin over the app, such as a banner. Touches and clicks reach the views of the overlay where they are and the app everywhere else. The overlay places its views again when the window changes size or turns, and a view added before the window exists shows once it does. On iOS, tvOS and Mac Catalyst the views never take the focus, so the remote of the TV and the keyboard keep driving the app.
NS_SWIFT_UI_ACTOR
@interface HaylenOverlay : NSObject

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

#if TARGET_OS_OSX
- (HaylenOverlayItem*)addView:(NSView*)view placement:(HaylenPlacement*)placement NS_SWIFT_NAME(add(_:placement:));
#else
- (HaylenOverlayItem*)addView:(UIView*)view placement:(HaylenPlacement*)placement NS_SWIFT_NAME(add(_:placement:));
#endif

@end

// A view that the overlay places. Its bounds are its frame over the app in points from the top left corner, and removing it gives back the edge it reserved.
NS_SWIFT_UI_ACTOR
NS_SWIFT_NAME(HaylenOverlay.Item)
@interface HaylenOverlayItem : NSObject

@property(nonatomic, getter=isVisible) BOOL visible;
@property(nonatomic, readonly) CGRect bounds;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

- (void)updatePlacement:(HaylenPlacement*)placement NS_SWIFT_NAME(update(_:));
- (void)remove;

@end

// What the runtime gives the native part of a plugin, once per plugin: its id, its parameters from `app.json` with the defaults of `plugin.json`, its overlay and the app's window. Methods, events, screens and streams take the id of the plugin in front of their names, so `registerHandler:@"show"` answers `<id>.show`. Registering, emitting, covering and opening streams work from any thread, while the window, the view controller and the overlay belong to the main thread.
@interface HaylenPluginContext : NSObject

@property(nonatomic, readonly, copy) NSString* identifier;
@property(nonatomic, readonly, copy) NSDictionary<NSString*, id>* config;
@property(nonatomic, readonly) HaylenOverlay* overlay;
#if TARGET_OS_OSX
@property(nonatomic, readonly, nullable) NSWindow* window NS_SWIFT_UI_ACTOR;
#else
// The topmost view controller that the window of the app presents, which is the root view controller while nothing is presented over the app.
@property(nonatomic, readonly, nullable) UIViewController* viewController NS_SWIFT_UI_ACTOR;
@property(nonatomic, readonly, nullable) UIWindowScene* windowScene NS_SWIFT_UI_ACTOR;
#endif

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

- (void)registerHandler:(NSString*)method handler:(HaylenHandler)handler;
- (void)registerCancellableHandler:(NSString*)method handler:(HaylenCancellableHandler)handler;

// Opens the screen `<id>.<name>` that the app asks for with `handle:openScreen(name, params, options)`, as `HaylenScreen` describes.
- (void)registerScreen:(NSString*)name handler:(HaylenScreenHandler)handler;

// Opens the video stream `name` of the plugin, which the app draws through `handle:videoStream(name)`, with its format and a size, 0 by 0 until the first frame, or returns the stream that is open already. Returns `nil` and logs why for an empty name, a negative size or a stream that is open with another format.
- (nullable HaylenVideoStream*)openVideoStream:(NSString*)name width:(NSInteger)width height:(NSInteger)height format:(HaylenVideoStreamFormat)format NS_SWIFT_NAME(openVideoStream(_:width:height:format:));

// Opens the audio stream `name` of the plugin, which the app plays through `handle:audioStream(name)`, with its sample rate, its channels, its format and room for `capacity` frames, or returns the stream that is open already. Returns `nil` and logs why for an empty name, a rate, channel count or capacity below 1, or a stream that is open with another rate, channel count or format.
- (nullable HaylenAudioStream*)openAudioStream:(NSString*)name sampleRate:(NSInteger)sampleRate channels:(NSInteger)channels format:(HaylenAudioStreamFormat)format capacity:(NSInteger)capacity NS_SWIFT_NAME(openAudioStream(_:sampleRate:channels:format:capacity:));

// A retained event waits for the first listener of its name, such as the link that opened the app, which arrives before the app listens. The batched events of a name that arrive in one frame reach the app as one list in order, such as the readings of a sensor. NSData values anywhere inside a payload cross as byte buffers.
- (void)emit:(NSString*)event payload:(nullable id)payload;
- (void)emitRetained:(NSString*)event payload:(nullable id)payload;
- (void)emit:(NSString*)event payload:(nullable id)payload retain:(BOOL)retain batched:(BOOL)batched;

// Native UI that covers the app, such as a full screen ad or a sign-in sheet, covers it while it shows, which halts and mutes the app. Covers are counted, and the covers of a plugin end when the window of the app goes away.
- (void)coverApp;
- (void)uncoverApp;

@end

// The native part of a plugin on Apple platforms. The runtime creates one instance of every class that the HaylenPlugins array of the Info.plist names with init and loads it while the app launches, before launching ends, so SDKs can set up there. It then hands each plugin the events of the app, its scene and its notifications on the main thread. The runtime owns the delegate of the notification center, so plugins implement its methods here instead of replacing it. Every method that takes a completion handler must call it once.
NS_SWIFT_UI_ACTOR
@protocol HaylenPlugin <NSObject>

- (void)loadWithContext:(HaylenPluginContext*)context;

@optional

#if TARGET_OS_OSX
- (void)applicationWillFinishLaunching:(NSNotification*)notification;
- (void)applicationDidFinishLaunching:(NSNotification*)notification;
- (void)application:(NSApplication*)application openURLs:(NSArray<NSURL*>*)urls;
- (void)application:(NSApplication*)application didRegisterForRemoteNotificationsWithDeviceToken:(NSData*)deviceToken;
- (void)application:(NSApplication*)application didFailToRegisterForRemoteNotificationsWithError:(NSError*)error;
- (void)application:(NSApplication*)application didReceiveRemoteNotification:(NSDictionary<NSString*, id>*)userInfo;
#else
- (void)application:(UIApplication*)application willFinishLaunchingWithOptions:(nullable NSDictionary<UIApplicationLaunchOptionsKey, id>*)launchOptions;
- (void)application:(UIApplication*)application didFinishLaunchingWithOptions:(nullable NSDictionary<UIApplicationLaunchOptionsKey, id>*)launchOptions;

// The runtime hands the links, the user activities and the shortcut item that open the app to the methods that receive them while it runs, right after this one.
- (void)scene:(UIScene*)scene willConnectToSession:(UISceneSession*)session options:(UISceneConnectionOptions*)connectionOptions;
- (void)scene:(UIScene*)scene openURLContexts:(NSSet<UIOpenURLContext*>*)URLContexts;
- (void)scene:(UIScene*)scene continueUserActivity:(NSUserActivity*)userActivity;
- (void)windowScene:(UIWindowScene*)windowScene performActionForShortcutItem:(UIApplicationShortcutItem*)shortcutItem completionHandler:(void (^)(BOOL succeeded))completionHandler API_UNAVAILABLE(tvos);
- (void)sceneDidBecomeActive:(UIScene*)scene;
- (void)sceneWillResignActive:(UIScene*)scene;
- (void)sceneWillEnterForeground:(UIScene*)scene;
- (void)sceneDidEnterBackground:(UIScene*)scene;

- (void)application:(UIApplication*)application didRegisterForRemoteNotificationsWithDeviceToken:(NSData*)deviceToken;
- (void)application:(UIApplication*)application didFailToRegisterForRemoteNotificationsWithError:(NSError*)error;
- (void)application:(UIApplication*)application didReceiveRemoteNotification:(NSDictionary*)userInfo fetchCompletionHandler:(void (^)(UIBackgroundFetchResult result))completionHandler;
- (void)application:(UIApplication*)application handleEventsForBackgroundURLSession:(NSString*)identifier completionHandler:(void (^)(void))completionHandler;
#endif

- (void)userNotificationCenter:(UNUserNotificationCenter*)center willPresentNotification:(UNNotification*)notification withCompletionHandler:(void (^)(UNNotificationPresentationOptions options))completionHandler;
- (void)userNotificationCenter:(UNUserNotificationCenter*)center didReceiveNotificationResponse:(UNNotificationResponse*)response withCompletionHandler:(void (^)(void))completionHandler API_UNAVAILABLE(tvos);

// Receives every error that stops the app, the one its error screen shows, as {message, file, line, traceback, frames} with frames of {source, line, function, kind}.
- (void)appDidFailWithError:(NSDictionary<NSString*, id>*)error NS_SWIFT_NAME(appDidFail(with:));

@end

NS_ASSUME_NONNULL_END
