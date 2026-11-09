#import "platform/apple/ApplePowerSaving.hpp"

#import <Foundation/Foundation.h>

#include "platform/NativeHost.hpp"
#include "platform/SystemState.hpp"

namespace haylen::platform {

void ApplePowerSaving::observe() {
    report();
    [NSNotificationCenter.defaultCenter addObserverForName:NSProcessInfoPowerStateDidChangeNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification*) { report(); }];
}

void ApplePowerSaving::report() {
    NativeHost::getSystemState().setPowerSaving(NSProcessInfo.processInfo.lowPowerModeEnabled == YES);
}

} // namespace haylen::platform
