#import "platform/apple/AppleSystem.hpp"

#include <TargetConditionals.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif

#include <sys/sysctl.h>

#include <utility>
#include <vector>

namespace haylen::platform {

// Mac Catalyst apps run on a Mac, so they report macOS with the model and the version of the Mac. The simulators run on the kernel of the Mac, whose machine is its processor, so they report the model they simulate.
SystemInfo AppleSystem::getInfo() {
    SystemInfo info;
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
    info.os = SystemInfo::Os::MacOs;
    info.deviceKind = SystemInfo::DeviceKind::Desktop;
    info.osVersion = readSysctl("kern.osproductversion");
    info.deviceModel = readSysctl("hw.model");
#else
#if TARGET_OS_TV
    info.os = SystemInfo::Os::TvOs;
    info.deviceKind = SystemInfo::DeviceKind::Tv;
#else
    const bool tablet = UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPad;
    info.os = tablet ? SystemInfo::Os::IpadOs : SystemInfo::Os::Ios;
    info.deviceKind = tablet ? SystemInfo::DeviceKind::Tablet : SystemInfo::DeviceKind::Phone;
#endif
    info.osVersion = UIDevice.currentDevice.systemVersion.UTF8String;
#if TARGET_OS_SIMULATOR
    NSString* simulated = NSProcessInfo.processInfo.environment[@"SIMULATOR_MODEL_IDENTIFIER"];
    info.deviceModel = simulated != nil ? simulated.UTF8String : "";
#else
    info.deviceModel = readSysctl("hw.machine");
#endif
#endif

    info.manufacturer = "Apple";
    info.cpuName = readSysctl("machdep.cpu.brand_string");
    info.cpuCores = static_cast<int>(NSProcessInfo.processInfo.processorCount);
    info.memoryBytes = NSProcessInfo.processInfo.physicalMemory;
    info.locale = getLanguageTag().UTF8String;
    for (NSString* language in NSLocale.preferredLanguages) {
        info.languages.emplace_back(language.UTF8String);
    }
    info.timeZone = NSTimeZone.localTimeZone.name.UTF8String;
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

// An iPhone plays an impact on its haptic engine, whatever the length of the vibration, and the other devices do nothing.
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

std::string AppleSystem::readSysctl(const char* name) {
    std::size_t size = 0;
    if (sysctlbyname(name, nullptr, &size, nullptr, 0) != 0 || size == 0) {
        return {};
    }
    std::vector<char> value(size);
    if (sysctlbyname(name, value.data(), &size, nullptr, 0) != 0) {
        return {};
    }
    return std::string(value.data());
}

} // namespace haylen::platform
