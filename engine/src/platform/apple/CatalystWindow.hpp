#pragma once

#include <TargetConditionals.h>

#if TARGET_OS_MACCATALYST
#import <UIKit/UIKit.h>

namespace haylen::platform {

// Opens the Mac Catalyst window at the size app.json gives the window, instead of the size the system gives every new iPad window on the Mac.
class CatalystWindow final {
  public:
    // Sizes the window of the scene that connects to the app, before it first shows.
    static void observe();

  private:
    static void resize(UIWindowScene* scene);
};

} // namespace haylen::platform
#endif
