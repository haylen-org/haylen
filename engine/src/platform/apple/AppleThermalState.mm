#import "platform/apple/AppleThermalState.hpp"

#import <Foundation/Foundation.h>

#include "haylen/platform/ThermalState.hpp"
#include "platform/NativeHost.hpp"
#include "platform/SystemState.hpp"

namespace haylen::platform {

void AppleThermalState::observe() {
    report();
    [NSNotificationCenter.defaultCenter addObserverForName:NSProcessInfoThermalStateDidChangeNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification*) { report(); }];
}

void AppleThermalState::report() {
    ThermalState state = ThermalState::Unknown;
    switch (NSProcessInfo.processInfo.thermalState) {
    case NSProcessInfoThermalStateNominal:
        state = ThermalState::Nominal;
        break;
    case NSProcessInfoThermalStateFair:
        state = ThermalState::Fair;
        break;
    case NSProcessInfoThermalStateSerious:
        state = ThermalState::Serious;
        break;
    case NSProcessInfoThermalStateCritical:
        state = ThermalState::Critical;
        break;
    }

    NativeHost::getSystemState().setThermalState(state);
}

} // namespace haylen::platform
