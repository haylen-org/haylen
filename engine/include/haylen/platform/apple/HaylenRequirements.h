#pragma once

#include <TargetConditionals.h>

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// Something that a feature needs from the Apple project of the app, with the file of the project that declares it and the snippet that adds it there.
@interface HaylenRequirement : NSObject

// One of `infoPlistKey`, `usageDescription`, `backgroundMode`, `urlScheme`, `class` and `entitlement`.
@property(nonatomic, readonly, copy) NSString* kind;

// The key, the mode, the scheme, the class or the entitlement.
@property(nonatomic, readonly, copy) NSString* name;

// The file of the Apple project that declares the requirement, relative to the project, such as `ios/Info.plist` on iOS and Mac Catalyst.
@property(nonatomic, readonly, copy) NSString* file;

@property(nonatomic, readonly, copy) NSString* snippet;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

// A key of the `Info.plist` with the text it takes, such as the app id of an SDK.
+ (instancetype)infoPlistKey:(NSString*)key value:(NSString*)value;

// The text of a permission prompt, such as `NSCameraUsageDescription`, without which the system ends the app at the call that asks for the permission.
+ (instancetype)usageDescription:(NSString*)key;

// A mode of `UIBackgroundModes`, such as `remote-notification`.
+ (instancetype)backgroundMode:(NSString*)mode;

// A scheme of `CFBundleURLTypes`, whose links open the app.
+ (instancetype)urlScheme:(NSString*)scheme;

// A class that a framework or a library brings, such as `UNUserNotificationCenter` of `UserNotifications.framework`, which the target of the app links.
+ (instancetype)className:(NSString*)name framework:(NSString*)framework;

// A boolean entitlement that the app signs with. Only macOS and Mac Catalyst tell an app its entitlements, so on iOS and tvOS the requirement never counts as missing, and the error of the API that needs it tells instead.
+ (instancetype)entitlement:(NSString*)key;

@end

// Tells what the Apple project of the app holds: the keys of its `Info.plist`, the classes it links and, on macOS and Mac Catalyst, the entitlements it signs with. The project belongs to its developer, so a plugin checks what it needs before it calls a system API that needs it, and a missing requirement logs once what is missing and how to add it and fails the call with the code `unsupported` instead of crashing the app. Every method works from any thread.
@interface HaylenRequirements : NSObject

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

- (BOOL)hasInfoPlistKey:(NSString*)key;

// Whether the `Info.plist` gives the key a text.
- (BOOL)hasUsageDescription:(NSString*)key;

- (BOOL)hasBackgroundMode:(NSString*)mode;

// Whether a scheme of `CFBundleURLTypes` matches, ignoring case.
- (BOOL)hasURLScheme:(NSString*)scheme;

// Whether the app links the class, which its framework or SDK brings.
- (BOOL)hasClass:(NSString*)name;

#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
// Whether the app signs with the entitlement and its value is not `false`.
- (BOOL)hasEntitlement:(NSString*)key;
#endif

// The requirements that the project lacks, in their order.
- (NSArray<HaylenRequirement*>*)missing:(NSArray<HaylenRequirement*>*)requirements;

// Returns `NO` with an error when the project lacks any of the requirements, after it logged each missing one once. The user info of the error holds the failure that a handler replies with, `message`, the code `unsupported` and `data`, whose `missing` lists each missing requirement as `{kind, name, file, snippet}`, so `reply(NO, error.userInfo)` fails the call.
- (BOOL)require:(NSArray<HaylenRequirement*>*)requirements error:(NSError**)error;

@end

NS_ASSUME_NONNULL_END
