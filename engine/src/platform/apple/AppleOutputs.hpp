#pragma once

#include <TargetConditionals.h>

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
#import <UIKit/UIKit.h>

#include <cstdint>
#include <map>
#include <string_view>

#include "haylen/gpu/Surface.hpp"
#include "platform/Host.hpp"

@class HaylenOutputView;

namespace haylen::platform {

// The outputs of the app on the external displays of an iPhone or an iPad, each a window in the scene that UIKit connects for its display, with a Metal layer that the GPU device draws into. A display without such a scene, such as one that Stage Manager extends the windows of the iPad to, shows no output. Everything runs on the main thread, which is the frame thread.
class AppleOutputs final {
  public:
    // Opens a window in the scene of the display, whose surface takes the samples and the vertical sync of the template.
    static void open(std::uint32_t number, std::string_view display, const gpu::Surface& surface);
    static void close(std::uint32_t number);
    [[nodiscard]] static Host::OutputReport getReport(std::uint32_t number);

    // Takes the new size of the view of an output.
    static void resize(std::uint32_t number);

    // Ends the outputs in a scene that disconnected, whose display went away.
    static void disconnect(UIScene* scene);

    // Shows black in the window of the scene of an external display, which shows no output.
    static void clear(UIWindow* window);

    // Registers on the view controller of the window of the app, or unregisters, the scene accessory through which iOS 27 and later connect the scene of an external display to an app, which earlier releases connect on their own. An external display shows the scene of the app while the accessory is registered, and a mirror of its window otherwise.
    static void setAccessory(bool value);

  private:
    struct Entry {
        UIWindowScene* scene = nil;
        UIWindow* window = nil;
        HaylenOutputView* view = nil;
        Host::OutputReport report;
    };

    static void announce(std::uint32_t number);

    static std::map<std::uint32_t, Entry>& entries;
    static NSObject* accessory;
    static UIViewController* accessoryController;
};

} // namespace haylen::platform
#endif
