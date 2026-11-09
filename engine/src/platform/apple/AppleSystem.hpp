#pragma once

#import <Foundation/Foundation.h>

#include <functional>
#include <string>

#include "haylen/platform/Permission.hpp"
#include "haylen/platform/SystemInfo.hpp"

namespace haylen::platform {

// The system of Apple platforms as the engine sees it: what the device is, opening urls with the app that handles them and the haptic feedback of iPhones. Every method runs on the main thread, which is the frame thread.
class AppleSystem final {
  public:
    [[nodiscard]] static SystemInfo getInfo();
    static void openUrl(const std::string& url, std::function<void(Permission::Answer answer)> callback);
    [[nodiscard]] static Permission::Answer vibrate();

  private:
    [[nodiscard]] static Permission::Answer answerOpened(bool opened, const std::string& url);

    // The text value of a `sysctl` name, empty where the kernel has none, such as the processor name on iPhones.
    [[nodiscard]] static std::string readSysctl(const char* name);
};

} // namespace haylen::platform
