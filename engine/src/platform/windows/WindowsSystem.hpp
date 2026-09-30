#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "haylen/platform/SystemInfo.hpp"

namespace haylen::platform {

// The system of Windows as the engine sees it: what the device is, the app theme of the personalization settings, the power status and opening urls with the app that handles them.
class WindowsSystem final {
  public:
    [[nodiscard]] static SystemInfo getInfo();
    static void openUrl(const std::string& url, std::function<void(bool opened)> callback);

    // Reports the theme and the battery to the engine, which the services do when the app starts and the window whenever Windows tells it that they changed.
    static void reportTheme();
    static void reportBattery();

  private:
    static constexpr const wchar_t* kBiosKey = L"HARDWARE\\DESCRIPTION\\System\\BIOS";
    static constexpr const wchar_t* kProcessorKey = L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";
    static constexpr const wchar_t* kPersonalizeKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";

    [[nodiscard]] static std::string getSystemVersion();
    [[nodiscard]] static std::string getLocale();
    [[nodiscard]] static std::vector<std::string> getLanguages();
    [[nodiscard]] static std::string getTimeZone();
    [[nodiscard]] static std::uint64_t getMemory();
    [[nodiscard]] static std::string readMachineText(const wchar_t* key, const wchar_t* name);
};

} // namespace haylen::platform
