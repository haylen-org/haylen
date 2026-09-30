#import "platform/apple/AppleScreens.hpp"

#include <string>

#include "haylen/core/Json.hpp"
#include "platform/ScreenRelay.hpp"
#import "platform/apple/AppleBridge.hpp"
#import "platform/apple/HaylenScreen+Runtime.h"
#include "sokol_app.h"

#if !TARGET_OS_OSX
#import "platform/apple/ApplePresenter.hpp"
#endif

namespace haylen::platform {

NSMutableDictionary<NSString*, HaylenScreenHandler>* AppleScreens::handlers = [NSMutableDictionary dictionary];
NSMutableDictionary<NSNumber*, HaylenScreen*>* AppleScreens::screens = [NSMutableDictionary dictionary];

#if !TARGET_OS_OSX && !TARGET_OS_TV
NSString* const AppleScreens::kWindowActivity = @"dev.haylen.screen";
#endif

void AppleScreens::registerScreen(NSString* key, HaylenScreenHandler handler) {
    @synchronized(handlers) {
        handlers[key] = [handler copy];
    }
}

// The handler runs on the main queue after the frame that covered the app, like the handlers of calls, and not at all for a screen that the app gave up before.
void AppleScreens::open(const ScreenRequest& request) {
    NSString* key = @((request.plugin + "." + request.screen).c_str());
    HaylenScreenHandler handler = nil;
    @synchronized(handlers) {
        handler = handlers[key];
    }
    if (handler == nil) {
        ScreenRelay::finish(request.id, false, core::Json{{"message", "No screen is registered for \"" + request.plugin + "." + request.screen + "\"."}, {"code", "noHandler"}}.dump());
        return;
    }
#if TARGET_OS_OSX
    const bool windowed = sapp_isvalid();
#else
    const bool windowed = ApplePresenter::getTopmost() != nil;
#endif
    if (!windowed) {
        ScreenRelay::finish(request.id, false, core::Json{{"message", "The app has no window to show the screen \"" + request.plugin + "." + request.screen + "\" over."}, {"code", "notActive"}}.dump());
        return;
    }

    HaylenScreen* screen = [[HaylenScreen alloc] initWithRequest:request];
    screens[@(request.id)] = screen;
    id params = AppleBridge::decode(request.params.json.dump(), request.params.buffers);
    dispatch_async(dispatch_get_main_queue(), ^{
      if (!screen.ended) {
          handler(params != nil ? params : @{}, screen);
      }
    });
}

void AppleScreens::cancel(std::uint64_t id) {
    [find(id) cancel];
}

HaylenScreen* AppleScreens::find(std::uint64_t id) {
    return screens[@(id)];
}

void AppleScreens::forget(std::uint64_t id) {
    [screens removeObjectForKey:@(id)];
}

} // namespace haylen::platform
