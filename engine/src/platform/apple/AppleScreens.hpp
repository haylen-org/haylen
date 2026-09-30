#pragma once

#include <TargetConditionals.h>

#import "haylen/platform/apple/HaylenScreen.h"

#include <cstdint>

#include "haylen/platform/ScreenRequest.hpp"

namespace haylen::platform {

// The screens that the plugin classes of Apple platforms register by `<id>.<name>`, and the screens that show. Opening a screen hands it to the handler of its plugin on the main queue, which shows its UI through the `HaylenScreen`, and every screen ends exactly once through `ScreenRelay`. Registering works from any thread, and everything else runs on the main thread, which is the frame thread.
class AppleScreens final {
  public:
    static void registerScreen(NSString* key, HaylenScreenHandler handler);

    // Fails the screen with the code `noHandler` when no plugin registered it, and with the code `notActive` while the app has no window to show it over.
    static void open(const ScreenRequest& request);

    // Gives up a screen that the app no longer waits for, as `HaylenScreen` describes.
    static void cancel(std::uint64_t id);

    // The screen that shows under the id, or `nil` once it ended.
    [[nodiscard]] static HaylenScreen* find(std::uint64_t id);
    static void forget(std::uint64_t id);

#if !TARGET_OS_OSX && !TARGET_OS_TV
    // The activity type that asks for the window of a screen, whose user info names the screen.
    static NSString* const kWindowActivity;
#endif

  private:
    static NSMutableDictionary<NSString*, HaylenScreenHandler>* handlers;
    static NSMutableDictionary<NSNumber*, HaylenScreen*>* screens;
};

} // namespace haylen::platform
