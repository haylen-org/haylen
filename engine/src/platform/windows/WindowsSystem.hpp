#pragma once

#include <functional>
#include <string>

#include "haylen/platform/SystemInfo.hpp"

namespace haylen::platform {

// The system of Windows as the engine sees it: what the device is and opening urls with the app that handles them.
class WindowsSystem final {
  public:
    [[nodiscard]] static SystemInfo getInfo();
    static void openUrl(const std::string& url, std::function<void(bool opened)> callback);

  private:
    [[nodiscard]] static std::string getSystemVersion();
    [[nodiscard]] static std::string getLocale();
    [[nodiscard]] static std::string narrow(const wchar_t* text);
    [[nodiscard]] static std::wstring widen(const std::string& text);
};

} // namespace haylen::platform
