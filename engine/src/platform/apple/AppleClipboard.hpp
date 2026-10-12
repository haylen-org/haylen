#pragma once

#include <TargetConditionals.h>

#if TARGET_OS_IOS
#include <string>
#include <string_view>

namespace haylen::platform {

// The general pasteboard of iPhone, iPad and Mac Catalyst apps, which `sokol_app` leaves out. Reading it shows the notice of iOS that the app pasted.
class AppleClipboard final {
  public:
    static void set(std::string_view text);
    [[nodiscard]] static std::string get();
};

} // namespace haylen::platform
#endif
