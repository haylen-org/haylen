#import "platform/apple/AppleSystem.hpp"

#include <TargetConditionals.h>

#if !TARGET_OS_OSX
#import <UIKit/UIKit.h>
#endif

#include <sys/sysctl.h>

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
    return info;
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
