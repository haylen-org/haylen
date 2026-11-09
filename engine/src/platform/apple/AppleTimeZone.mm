#import "platform/apple/AppleTimeZone.hpp"

#import <Foundation/Foundation.h>

#include "platform/NativeHost.hpp"
#include "platform/SystemState.hpp"

namespace haylen::platform {

void AppleTimeZone::observe() {
    report();
    [NSNotificationCenter.defaultCenter addObserverForName:NSSystemTimeZoneDidChangeNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification*) { report(); }];
}

// Foundation keeps the time zone it read first until it is reset.
void AppleTimeZone::report() {
    [NSTimeZone resetSystemTimeZone];
    NativeHost::getSystemState().setTimeZone(NSTimeZone.systemTimeZone.name.UTF8String);
}

} // namespace haylen::platform
