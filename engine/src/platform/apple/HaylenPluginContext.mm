#import "platform/apple/HaylenPluginContext+Runtime.h"

#include "haylen/core/Log.hpp"
#import "platform/apple/AppleBridge.hpp"
#import "platform/apple/HaylenOverlay+Runtime.h"
#include "platform/sokol/SokolHost.hpp"
#include "sokol_app.h"

using haylen::platform::AppleBridge;
using haylen::platform::SokolHost;

@interface HaylenPluginContext ()

@property(nonatomic, readwrite, copy) NSString* identifier;
@property(nonatomic, readwrite, copy) NSDictionary<NSString*, id>* config;
@property(nonatomic, readwrite) HaylenOverlay* overlay;

@end

@implementation HaylenPluginContext {
    NSInteger covers;
}

- (instancetype)initWithIdentifier:(NSString*)plugin config:(NSDictionary<NSString*, id>*)values {
    self = [super init];
    self.identifier = plugin;
    self.config = values;
    self.overlay = [[HaylenOverlay alloc] initWithIdentifier:plugin];
    return self;
}

#if TARGET_OS_OSX
- (NSWindow*)window {
    return sapp_isvalid() ? (__bridge NSWindow*)sapp_macos_get_window() : nil;
}
#else
- (UIViewController*)viewController {
    return sapp_isvalid() ? ((__bridge UIWindow*)sapp_ios_get_window()).rootViewController : nil;
}

- (UIWindowScene*)windowScene {
    return sapp_isvalid() ? ((__bridge UIWindow*)sapp_ios_get_window()).windowScene : nil;
}
#endif

- (void)registerHandler:(NSString*)method handler:(HaylenHandler)handler {
    [HaylenBridge registerHandler:[self qualify:method] handler:handler];
}

- (void)registerCancellableHandler:(NSString*)method handler:(HaylenCancellableHandler)handler {
    [HaylenBridge registerCancellableHandler:[self qualify:method] handler:handler];
}

- (void)emit:(NSString*)event payload:(id)payload {
    AppleBridge::emit([self qualify : event], payload, {});
}

- (void)emitRetained:(NSString*)event payload:(id)payload {
    AppleBridge::emit([self qualify : event], payload, { .retain = true });
}

- (void)emit:(NSString*)event payload:(id)payload retain:(BOOL)retain batched:(BOOL)batched {
    AppleBridge::emit([self qualify : event], payload, { .retain = retain == YES, .batched = batched == YES });
}

- (void)coverApp {
    @synchronized(self) {
        ++covers;
    }
    SokolHost::getNativeViews().coverApp();
}

- (void)uncoverApp {
    @synchronized(self) {
        if (covers == 0) {
            haylen::core::Log::error("The plugin {} uncovered the app without covering it first.", self.identifier.UTF8String);
            return;
        }
        --covers;
    }
    SokolHost::getNativeViews().uncoverApp();
}

- (void)closeCovers {
    NSInteger open = 0;
    @synchronized(self) {
        open = covers;
        covers = 0;
    }
    for (NSInteger index = 0; index < open; ++index) {
        SokolHost::getNativeViews().uncoverApp();
    }
}

- (NSString*)qualify:(NSString*)name {
    return [NSString stringWithFormat:@"%@.%@", self.identifier, name];
}

@end
