#import "platform/apple/AppleBattery.hpp"

#import <Foundation/Foundation.h>

#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
#import <IOKit/ps/IOPSKeys.h>
#import <IOKit/ps/IOPowerSources.h>
#elif TARGET_OS_IOS
#import <UIKit/UIKit.h>
#endif

#include "platform/SystemState.hpp"
#include "platform/sokol/SokolHost.hpp"

namespace haylen::platform {

#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
void AppleBattery::observe() {
    SokolHost::getSystemState().setBattery(readPowerSources());
    CFRunLoopSourceRef source = IOPSNotificationCreateRunLoopSource(&powerSourcesChanged, nullptr);
    CFRunLoopAddSource(CFRunLoopGetMain(), source, kCFRunLoopDefaultMode);
    CFRelease(source);
}

void AppleBattery::powerSourcesChanged(void*) {
    SokolHost::getSystemState().setBattery(readPowerSources());
}

// A Mac on mains power that holds its battery below full, as optimized charging does, neither charges nor discharges, which counts as full.
Battery AppleBattery::readPowerSources() {
    CFTypeRef info = IOPSCopyPowerSourcesInfo();
    CFArrayRef sources = IOPSCopyPowerSourcesList(info);
    Battery battery{.state = Battery::State::None};
    for (CFIndex index = 0; index < CFArrayGetCount(sources); ++index) {
        NSDictionary* values = (__bridge NSDictionary*)IOPSGetPowerSourceDescription(info, CFArrayGetValueAtIndex(sources, index));
        if (![values[@kIOPSTypeKey] isEqualToString:@kIOPSInternalBatteryType]) {
            continue;
        }
        const double current = [values[@kIOPSCurrentCapacityKey] doubleValue];
        const double maximum = [values[@kIOPSMaxCapacityKey] doubleValue];
        const bool external = [values[@kIOPSPowerSourceStateKey] isEqualToString:@kIOPSACPowerValue];
        battery.charging = [values[@kIOPSIsChargingKey] boolValue];
        if (maximum > 0.0) {
            battery.level = static_cast<float>(current / maximum);
        }
        if (battery.charging) {
            battery.state = Battery::State::Charging;
        } else {
            battery.state = external ? Battery::State::Full : Battery::State::Discharging;
        }
        break;
    }
    CFRelease(sources);
    CFRelease(info);
    return battery;
}
#elif TARGET_OS_IOS
// The class `UIDevice` reports the level in steps of 5 percent and posts its notifications about once a minute.
void AppleBattery::observe() {
    UIDevice.currentDevice.batteryMonitoringEnabled = YES;
    SokolHost::getSystemState().setBattery(readDevice());
    NSNotificationCenter* center = NSNotificationCenter.defaultCenter;
    for (NSNotificationName name in @[ UIDeviceBatteryLevelDidChangeNotification, UIDeviceBatteryStateDidChangeNotification ]) {
        [center addObserverForName:name object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification*) { SokolHost::getSystemState().setBattery(readDevice()); }];
    }
}

Battery AppleBattery::readDevice() {
    UIDevice* device = UIDevice.currentDevice;
    Battery battery;
    if (device.batteryLevel >= 0.0F) {
        battery.level = device.batteryLevel;
    }
    switch (device.batteryState) {
    case UIDeviceBatteryStateUnknown:
        battery.state = Battery::State::Unknown;
        break;
    case UIDeviceBatteryStateUnplugged:
        battery.state = Battery::State::Discharging;
        break;
    case UIDeviceBatteryStateCharging:
        battery.state = Battery::State::Charging;
        battery.charging = true;
        break;
    case UIDeviceBatteryStateFull:
        battery.state = Battery::State::Full;
        break;
    }
    return battery;
}
#else
void AppleBattery::observe() {
    SokolHost::getSystemState().setBattery({.state = Battery::State::None});
}
#endif

} // namespace haylen::platform
