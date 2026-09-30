#pragma once

#import <Foundation/Foundation.h>

#include <functional>
#include <string>

#include "haylen/platform/SystemInfo.hpp"

namespace haylen::platform {

// The system of Apple platforms as the engine sees it: what the device is, opening urls with the app that handles them and the haptic feedback of iPhones. Every method runs on the main thread, which is the frame thread.
class AppleSystem final {
  public:
    [[nodiscard]] static SystemInfo getInfo();
    static void openUrl(const std::string& url, std::function<void(bool opened)> callback);
    static void vibrate();

  private:
    // The first preferred language as a BCP 47 tag, the format every platform reports.
    [[nodiscard]] static NSString* getLanguageTag();
};

} // namespace haylen::platform
