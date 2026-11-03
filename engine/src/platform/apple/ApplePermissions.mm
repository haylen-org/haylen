#import "platform/apple/ApplePermissions.hpp"

#include <TargetConditionals.h>
#include <dlfcn.h>

#import <AVFoundation/AVFoundation.h>
#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#endif

#include <format>
#include <utility>

#include "haylen/core/Log.hpp"
#include "platform/NativeWindow.hpp"
#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
#import "platform/apple/AppleOutputs.hpp"
#endif
#if !TARGET_OS_OSX
#import "platform/apple/AppleMotion.hpp"
#endif
#import "platform/apple/HaylenLocationAuthorization.h"
#import "platform/apple/HaylenRequirements+Runtime.h"

// The methods of `UNUserNotificationCenter`, `UNNotificationSettings` and `PHPhotoLibrary` that the runtime calls, with plain types in place of the types of their frameworks.
@protocol HaylenNotificationAuthorizer <NSObject>
+ (id)currentNotificationCenter;
- (void)getNotificationSettingsWithCompletionHandler:(void (^)(id settings))completionHandler;
- (void)requestAuthorizationWithOptions:(NSUInteger)options completionHandler:(void (^)(BOOL granted, NSError* _Nullable error))completionHandler;
@end

@protocol HaylenNotificationSettings <NSObject>
- (NSInteger)authorizationStatus;
@end

@protocol HaylenPhotoLibrary <NSObject>
+ (NSInteger)authorizationStatusForAccessLevel:(NSInteger)accessLevel;
+ (void)requestAuthorizationForAccessLevel:(NSInteger)accessLevel handler:(void (^)(NSInteger status))handler;
@end

namespace haylen::platform {

void ApplePermissions::check(Permission::Kind kind, std::function<void(Permission::Answer answer)> callback) {
    switch (kind) {
    case Permission::Kind::Camera:
    case Permission::Kind::Microphone:
        callback(checkCapture(kind));
        return;
    case Permission::Kind::Location:
    case Permission::Kind::PreciseLocation:
    case Permission::Kind::BackgroundLocation:
        callback(checkLocation(kind));
        return;
    case Permission::Kind::Notifications:
        checkNotifications(std::move(callback));
        return;
    case Permission::Kind::Photos:
        callback(checkPhotos());
        return;
    case Permission::Kind::Clipboard:
        callback(checkClipboard());
        return;
    case Permission::Kind::Motion:
        // A Mac reads the motion of controllers alone, which needs no permission, while Core Motion asks for the motion of the device from version 27.2.
#if TARGET_OS_OSX
        callback({.state = Permission::State::Granted});
#else
        callback({.state = AppleMotion::getPermission()});
#endif
        return;
    case Permission::Kind::Screens:
        callback({.state = Permission::State::Granted});
        return;
    case Permission::Kind::LocalNetwork:
        // Apple platforms ask the person for the local network the first time the app reaches it, and tell apps neither the answer nor a way to ask before.
        callback({.state = Permission::State::Unknown});
        return;
    }
}

// A permission whose system shows no prompt, or that the person already answered, answers its state at once.
void ApplePermissions::request(Permission::Kind kind, std::function<void(Permission::Answer answer)> callback) {
    switch (kind) {
    case Permission::Kind::Camera:
    case Permission::Kind::Microphone:
        requestCapture(kind, std::move(callback));
        return;
    case Permission::Kind::Location:
    case Permission::Kind::PreciseLocation:
    case Permission::Kind::BackgroundLocation:
        requestLocation(kind, std::move(callback));
        return;
    case Permission::Kind::Notifications:
        requestNotifications(std::move(callback));
        return;
    case Permission::Kind::Photos:
        requestPhotos(std::move(callback));
        return;
    case Permission::Kind::Motion:
#if TARGET_OS_OSX
        check(kind, std::move(callback));
#else
        AppleMotion::authorize(std::move(callback));
#endif
        return;
    case Permission::Kind::Clipboard:
    case Permission::Kind::Screens:
        // An iPhone or an iPad of iOS 27 and later mirrors an external display until the app registers the scene accessory that connects the scene of that display, which needs no answer of the person.
#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
        if (UIWindow* window = (__bridge UIWindow*)NativeWindow::getHandle(); window.rootViewController != nil) {
            AppleOutputs::registerAccessory(window.rootViewController);
        }
#endif
        check(kind, std::move(callback));
        return;
    case Permission::Kind::LocalNetwork:
        check(kind, std::move(callback));
        return;
    }
}

Permission::Answer ApplePermissions::checkCapture(Permission::Kind kind) {
    if (@available(tvOS 17.0, *)) {
        switch ([AVCaptureDevice authorizationStatusForMediaType:kind == Permission::Kind::Camera ? AVMediaTypeVideo : AVMediaTypeAudio]) {
        case AVAuthorizationStatusNotDetermined:
            return {.state = Permission::State::Prompt};
        case AVAuthorizationStatusAuthorized:
            return {.state = Permission::State::Granted};
        case AVAuthorizationStatusRestricted:
        case AVAuthorizationStatusDenied:
            break;
        }
        return {.state = Permission::State::Denied};
    }
    return unsupported(kind == Permission::Kind::Camera ? "Apple TV before tvOS 17 has no camera." : "Apple TV before tvOS 17 has no microphone for apps.");
}

void ApplePermissions::requestCapture(Permission::Kind kind, std::function<void(Permission::Answer answer)> callback) {
    const Permission::Answer current = checkCapture(kind);
    if (current.state != Permission::State::Prompt) {
        callback(current);
        return;
    }
    if (std::optional<Permission::Answer> missing = findMissingUsage(kind == Permission::Kind::Camera ? "NSCameraUsageDescription" : "NSMicrophoneUsageDescription")) {
        callback(std::move(*missing));
        return;
    }
    if (@available(tvOS 17.0, *)) {
        __block std::function<void(Permission::Answer)> done = std::move(callback);
        [AVCaptureDevice requestAccessForMediaType:kind == Permission::Kind::Camera ? AVMediaTypeVideo : AVMediaTypeAudio completionHandler:^(BOOL granted) { done({.state = granted ? Permission::State::Granted : Permission::State::Denied}); }];
    }
}

// The status counts as `CLAuthorizationStatus`: 0 not determined, 1 restricted, 2 denied, 3 always and 4 while in use, and the accuracy as `CLAccuracyAuthorization`: 0 full and 1 reduced. A Mac knows no location in the background apart from the one in use.
Permission::Answer ApplePermissions::checkLocation(Permission::Kind kind) {
#if TARGET_OS_TV || TARGET_OS_VISION
    if (kind == Permission::Kind::BackgroundLocation) {
        return unsupported(TARGET_OS_TV ? "Apple TV reads the location only while the app is in use." : "Apple Vision Pro reads the location only while the app is in use.");
    }
#endif
    HaylenLocationAuthorization* authorization = HaylenLocationAuthorization.shared;
    if (authorization == nil) {
        return unsupported("Core Location does not load on this device.");
    }

    const int status = authorization.status;
    if (status == 0) {
        return {.state = Permission::State::Prompt};
    }
    if (status != 3 && status != 4) {
        return {.state = Permission::State::Denied};
    }
    if (kind == Permission::Kind::PreciseLocation) {
        return {.state = authorization.accuracy == 0 ? Permission::State::Granted : Permission::State::Denied};
    }
#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
    if (kind == Permission::Kind::BackgroundLocation && status == 4) {
        return {
            .state = [NSUserDefaults.standardUserDefaults boolForKey:@(kAskedAlways)] ? Permission::State::Denied : Permission::State::Prompt
        };
    }
#endif
    return {.state = Permission::State::Granted};
}

void ApplePermissions::requestLocation(Permission::Kind kind, std::function<void(Permission::Answer answer)> callback) {
    const Permission::Answer current = checkLocation(kind);
    const bool reduced = kind == Permission::Kind::PreciseLocation && current.state == Permission::State::Denied && (HaylenLocationAuthorization.shared.status == 3 || HaylenLocationAuthorization.shared.status == 4);
    if (current.state != Permission::State::Prompt && !reduced) {
        callback(current);
        return;
    }
    if (std::optional<Permission::Answer> missing = findMissingUsage(TARGET_OS_OSX ? "NSLocationUsageDescription" : "NSLocationWhenInUseUsageDescription")) {
        callback(std::move(*missing));
        return;
    }

    __block std::function<void(Permission::Answer)> done = std::move(callback);
    void (^answer)(void) = ^{ done(checkLocation(kind)); };
    HaylenLocationAuthorization* authorization = HaylenLocationAuthorization.shared;
    if (reduced) {
        if (std::optional<Permission::Answer> missing = findMissingPurpose()) {
            done(std::move(*missing));
            return;
        }
        [authorization requestFullAccuracy:@(kPrecisePurpose) completion:answer];
        return;
    }
#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
    if (kind == Permission::Kind::BackgroundLocation) {
        if (std::optional<Permission::Answer> missing = findMissingUsage("NSLocationAlwaysAndWhenInUseUsageDescription")) {
            done(std::move(*missing));
            return;
        }
        [NSUserDefaults.standardUserDefaults setBool:YES forKey:@(kAskedAlways)];
        [authorization requestAlways:answer];
        return;
    }
#endif
    [authorization requestWhenInUse:answer];
}

// The status counts as `UNAuthorizationStatus`: 0 not determined, 1 denied, 2 authorized, 3 provisional and 4 ephemeral. The notification center of a process without a bundle, such as a player run from a terminal, raises an exception, so it answers unsupported instead.
void ApplePermissions::checkNotifications(std::function<void(Permission::Answer answer)> callback) {
    if (NSBundle.mainBundle.bundleIdentifier == nil) {
        callback(unsupported("The app runs without a bundle, so the system keeps no notifications for it."));
        return;
    }
    if (!loadFramework("UserNotifications", "UNUserNotificationCenter")) {
        callback(unsupported("UserNotifications does not load on this device."));
        return;
    }
    __block std::function<void(Permission::Answer)> done = std::move(callback);
    // clang-format off
    [[(Class<HaylenNotificationAuthorizer>)NSClassFromString(@"UNUserNotificationCenter") currentNotificationCenter] getNotificationSettingsWithCompletionHandler:^(id settings) {
        const NSInteger status = [(id<HaylenNotificationSettings>)settings authorizationStatus];
        done({.state = status == 0 ? Permission::State::Prompt : status == 1 ? Permission::State::Denied : Permission::State::Granted});
    }];
    // clang-format on
}

// Apple TV shows no notifications and only a badge on the icon of the app, which is all it asks for.
void ApplePermissions::requestNotifications(std::function<void(Permission::Answer answer)> callback) {
    // clang-format off
    checkNotifications([callback = std::move(callback)](Permission::Answer current) {
        if (current.state != Permission::State::Prompt) {
            callback(std::move(current));
            return;
        }
        const NSUInteger options = TARGET_OS_TV ? 1 : 1 | 2 | 4;
        [[(Class<HaylenNotificationAuthorizer>)NSClassFromString(@"UNUserNotificationCenter") currentNotificationCenter] requestAuthorizationWithOptions:options completionHandler:^(BOOL, NSError*) {
            checkNotifications(callback);
        }];
    });
    // clang-format on
}

// The status counts as `PHAuthorizationStatus`: 0 not determined, 1 restricted, 2 denied, 3 authorized and 4 limited, where the person picked the photos the app sees. The access level 2 is reading and writing the library.
Permission::Answer ApplePermissions::checkPhotos() {
    if (!loadFramework("Photos", "PHPhotoLibrary")) {
        return unsupported("Photos does not load on this device.");
    }
    const NSInteger status = [(Class<HaylenPhotoLibrary>)NSClassFromString(@"PHPhotoLibrary") authorizationStatusForAccessLevel:2];
    if (status == 0) {
        return {.state = Permission::State::Prompt};
    }
    return {.state = status == 3 || status == 4 ? Permission::State::Granted : Permission::State::Denied};
}

void ApplePermissions::requestPhotos(std::function<void(Permission::Answer answer)> callback) {
    const Permission::Answer current = checkPhotos();
    if (current.state != Permission::State::Prompt) {
        callback(current);
        return;
    }
    if (std::optional<Permission::Answer> missing = findMissingUsage("NSPhotoLibraryUsageDescription")) {
        callback(std::move(*missing));
        return;
    }
    __block std::function<void(Permission::Answer)> done = std::move(callback);
    [(Class<HaylenPhotoLibrary>)NSClassFromString(@"PHPhotoLibrary") requestAuthorizationForAccessLevel:2 handler:^(NSInteger) { done(checkPhotos()); }];
}

// The pasteboard of macOS 15.4 and later follows the choice of the person in the settings of the system, while iOS, iPadOS, visionOS and Mac Catalyst ask at each paste and keep the choice where apps cannot read it. An Apple TV keeps the clipboard inside the app.
Permission::Answer ApplePermissions::checkClipboard() {
#if TARGET_OS_OSX
    if (@available(macOS 15.4, *)) {
        switch (NSPasteboard.generalPasteboard.accessBehavior) {
        case NSPasteboardAccessBehaviorAsk:
            return {.state = Permission::State::Prompt};
        case NSPasteboardAccessBehaviorAlwaysAllow:
            return {.state = Permission::State::Granted};
        case NSPasteboardAccessBehaviorAlwaysDeny:
            return {.state = Permission::State::Denied};
        case NSPasteboardAccessBehaviorDefault:
            break;
        }
        return {.state = Permission::State::Unknown};
    }
    return {.state = Permission::State::Granted};
#elif TARGET_OS_TV
    return {.state = Permission::State::Granted};
#else
    return {.state = Permission::State::Unknown};
#endif
}

std::optional<Permission::Answer> ApplePermissions::findMissingUsage(const char* key) {
    static HaylenRequirements* const requirements = [[HaylenRequirements alloc] initWithOwner:@"The engine"];
    NSError* error = nil;
    if ([requirements require:@[ [HaylenRequirement usageDescription:@(key)] ] error:&error]) {
        return std::nullopt;
    }

    Permission::Answer answer{.state = Permission::State::Unsupported, .message = [error.userInfo[@"message"] UTF8String]};
    for (NSDictionary<NSString*, NSString*>* entry in error.userInfo[@"data"][@"missing"]) {
        answer.missing.push_back({.kind = entry[@"kind"].UTF8String, .name = entry[@"name"].UTF8String, .file = entry[@"file"].UTF8String, .snippet = entry[@"snippet"].UTF8String});
    }
    return answer;
}

// The temporary request for the precise location shows the text of its purpose key, without which the system refuses it.
std::optional<Permission::Answer> ApplePermissions::findMissingPurpose() {
    id purposes = NSBundle.mainBundle.infoDictionary[@"NSLocationTemporaryUsageDescriptionDictionary"];
    id text = [purposes isKindOfClass:NSDictionary.class] ? purposes[@(kPrecisePurpose)] : nil;
    if ([text isKindOfClass:NSString.class] && [text length] > 0) {
        return std::nullopt;
    }

#if TARGET_OS_OSX
    const char* file = "macos/Info.plist";
#elif TARGET_OS_TV
    const char* file = "tvos/Info.plist";
#else
    const char* file = "ios/Info.plist";
#endif
    const std::string snippet = std::format("<key>NSLocationTemporaryUsageDescriptionDictionary</key><dict><key>{}</key><string>Tell the person why the app asks for this.</string></dict>", kPrecisePurpose);
    static bool logged = false;
    if (!logged) {
        logged = true;
        core::Log::warning("The engine needs the key \"{}\" of \"NSLocationTemporaryUsageDescriptionDictionary\", which the app lacks, so the request for the precise location fails with the code \"unsupported\". Add \"{}\" to \"{}\".", kPrecisePurpose, snippet, file);
    }
    return Permission::Answer{.state = Permission::State::Unsupported, .missing = {{.kind = "infoPlistKey", .name = "NSLocationTemporaryUsageDescriptionDictionary", .file = file, .snippet = snippet}}, .message = std::format("The engine needs the key \"{}\" of \"NSLocationTemporaryUsageDescriptionDictionary\", which the app lacks.", kPrecisePurpose)};
}

bool ApplePermissions::loadFramework(const char* framework, const char* className) {
    const std::string path = std::format("/System/Library/Frameworks/{0}.framework/{0}", framework);
    return dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL) != nullptr && NSClassFromString(@(className)) != nil;
}

Permission::Answer ApplePermissions::unsupported(std::string message) {
    return {.state = Permission::State::Unsupported, .message = std::move(message)};
}

} // namespace haylen::platform
