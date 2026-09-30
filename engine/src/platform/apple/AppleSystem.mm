#import "platform/apple/AppleSystem.hpp"

#include <TargetConditionals.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif

#include <thread>
#include <utility>

namespace haylen::platform {

SystemInfo AppleSystem::getInfo() {
    SystemInfo info;
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
    info.os = SystemInfo::Os::MacOs;
    info.deviceKind = SystemInfo::DeviceKind::Desktop;
#elif TARGET_OS_TV
    info.os = SystemInfo::Os::TvOs;
    info.deviceKind = SystemInfo::DeviceKind::Tv;
#else
    const bool tablet = UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPad;
    info.os = tablet ? SystemInfo::Os::IpadOs : SystemInfo::Os::Ios;
    info.deviceKind = tablet ? SystemInfo::DeviceKind::Tablet : SystemInfo::DeviceKind::Phone;
#endif
#if TARGET_OS_OSX
    const NSOperatingSystemVersion version = NSProcessInfo.processInfo.operatingSystemVersion;
    info.osVersion = [NSString stringWithFormat:@"%ld.%ld.%ld", static_cast<long>(version.majorVersion), static_cast<long>(version.minorVersion), static_cast<long>(version.patchVersion)].UTF8String;
#else
    info.osVersion = UIDevice.currentDevice.systemVersion.UTF8String;
#endif
    info.cpuCores = static_cast<int>(std::thread::hardware_concurrency());
    info.locale = getLanguageTag().UTF8String;
    return info;
}

void AppleSystem::openUrl(const std::string& url, std::function<void(bool opened)> callback) {
    NSString* text = [[NSString alloc] initWithBytes:url.data() length:url.size() encoding:NSUTF8StringEncoding];
    NSURL* address = text != nil ? [NSURL URLWithString:text] : nil;
    if (address == nil) {
        callback(false);
        return;
    }
#if TARGET_OS_OSX
    callback([NSWorkspace.sharedWorkspace openURL:address]);
#else
    __block std::function<void(bool)> done = std::move(callback);
    [UIApplication.sharedApplication openURL:address options:@{} completionHandler:^(BOOL opened) { done(opened); }];
#endif
}

// iPhones play an impact on their haptic engine, whatever the length of the vibration, and the other devices do nothing.
void AppleSystem::vibrate() {
#if TARGET_OS_IOS
    UIImpactFeedbackGenerator* generator = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleMedium];
    [generator impactOccurred];
#endif
}

NSString* AppleSystem::getLanguageTag() {
    NSString* preferred = NSLocale.preferredLanguages.firstObject;
    return preferred != nil ? preferred : [NSLocale.currentLocale.localeIdentifier stringByReplacingOccurrencesOfString:@"_" withString:@"-"];
}

} // namespace haylen::platform
