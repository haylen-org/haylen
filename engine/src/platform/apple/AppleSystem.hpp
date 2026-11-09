#pragma once

#import <Foundation/Foundation.h>

#include <string>

#include "haylen/platform/SystemInfo.hpp"

namespace haylen::platform {

// What the Apple device that runs the app is, as the engine reports it. Every method runs on the main thread, which is the frame thread.
class AppleSystem final {
  public:
    [[nodiscard]] static SystemInfo getInfo();

  private:
    // The text value of a `sysctl` name, empty where the kernel has none, such as the processor name on iPhones.
    [[nodiscard]] static std::string readSysctl(const char* name);
};

} // namespace haylen::platform
