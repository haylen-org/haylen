#pragma once

#import <Foundation/Foundation.h>

#import "haylen/platform/apple/HaylenRequirements.h"

NS_ASSUME_NONNULL_BEGIN

@class HaylenPermission;

// The states of `system.permission`: the app holds it, asking may show the prompt of the system, the person refused it, the system tells nothing until the app tries, or the device has no such capability, which the message of the answer explains.
typedef NS_ENUM(NSInteger, HaylenPermissionState) {
    HaylenPermissionStateGranted,
    HaylenPermissionStatePrompt,
    HaylenPermissionStateDenied,
    HaylenPermissionStateUnknown,
    HaylenPermissionStateUnsupported,
} NS_SWIFT_NAME(HaylenPermission.State);

// Answers the state of a permission once, from any thread, with the sentence that says why for `HaylenPermissionStateUnsupported`.
typedef void (^HaylenPermissionAnswer)(HaylenPermissionState state, NSString* _Nullable message);

// A permission that a plugin owns, such as `camera`, which the plugin declares in the `permissions` of its `plugin.json` and registers with `registerPermission:` of its context. The runtime then answers `system.permission` and `system.requestPermission` for it: it checks the requirements first, which answer `unsupported` with what the project lacks, reads the state with the `state` block, and runs the `request` block only while the state is `prompt`. Both blocks run on the main thread and call their answer once, from any thread.
@interface HaylenPermission : NSObject

// The name of `system.permission`, such as `camera`.
@property(nonatomic, readonly, copy) NSString* name;

// What the permission gives the app in words, such as `the camera`, for the sentences of answers.
@property(nonatomic, readonly, copy) NSString* capability;

// What the project of the app needs before the system shows the prompt, such as the usage description of the `Info.plist`.
@property(nonatomic, readonly, copy) NSArray<HaylenRequirement*>* requirements;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

- (instancetype)initWithName:(NSString*)name capability:(NSString*)capability requirements:(NSArray<HaylenRequirement*>*)requirements state:(void (^)(HaylenPermissionAnswer answer))state request:(void (^)(HaylenPermissionAnswer answer))request NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END
