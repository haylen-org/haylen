#import <Foundation/Foundation.h>

#import "HaylenApp-Swift.h"

// Registers the handlers of `AppBridge.swift` when the runtime loads this class, before the app starts, since a Swift class cannot run code when it loads.
@interface AppBridgeLoader : NSObject
@end

@implementation AppBridgeLoader

+ (void)load {
    [AppBridge registerHandlers];
}

@end
