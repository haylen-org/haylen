#pragma once

#include <functional>
#include <string>

#include "haylen/platform/Permission.hpp"

namespace haylen::platform {

// The general pasteboard of macOS, iOS, iPadOS, Mac Catalyst and visionOS as the app reads it. Those systems may ask the person before an app reads text that another app wrote, and the read waits for the answer, so it runs away from the frame thread.
class ApplePasteboard final {
  public:
    // Calls back exactly once, on any thread, with `Granted` and the text, an empty text when the pasteboard holds none, or `Denied` when the person did not allow the app to paste.
    static void read(std::function<void(Permission::Answer answer, std::string text)> callback);
};

} // namespace haylen::platform
