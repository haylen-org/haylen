#pragma once

#include <functional>
#include <string>

#include "haylen/platform/SystemInfo.hpp"

namespace haylen::platform {

// The system of Linux as the engine sees it: what the device is and opening urls with the app that handles them.
class LinuxSystem final {
  public:
    [[nodiscard]] static SystemInfo getInfo();
    static void openUrl(const std::string& url, std::function<void(bool opened)> callback);

  private:
    [[nodiscard]] static std::string getSystemVersion();
    [[nodiscard]] static std::string getLocale();
};

} // namespace haylen::platform
