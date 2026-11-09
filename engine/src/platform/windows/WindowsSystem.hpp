#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "haylen/platform/Permission.hpp"
#include "haylen/platform/SystemInfo.hpp"

namespace haylen::platform {

// The system of Windows as the engine sees it: what the device is, the app theme of the personalization settings, the accessibility settings, the power status, the time zone and opening urls with the app that handles them.
class WindowsSystem final {
  public:
    [[nodiscard]] static SystemInfo getInfo();
    static void openUrl(const std::string& url, std::function<void(Permission::Answer answer)> callback);

    // Reports the theme, the accessibility settings, the battery with the battery saver and the time zone to the engine, which the services do when the app starts and the window whenever Windows tells it that they changed.
    static void reportTheme();
    static void reportAccessibility();
    static void reportPowerStatus();
    static void reportTimeZone();

  private:
    static constexpr std::uint8_t kPowerSaverOn = 1;
    static constexpr const wchar_t* kBiosKey = L"HARDWARE\\DESCRIPTION\\System\\BIOS";
    static constexpr const wchar_t* kProcessorKey = L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";

    [[nodiscard]] static std::string getSystemVersion();
    [[nodiscard]] static std::string getLocale();
    [[nodiscard]] static std::vector<std::string> getLanguages();
    [[nodiscard]] static std::string getTimeZone();
    [[nodiscard]] static std::uint64_t getMemory();
    [[nodiscard]] static std::string readMachineText(const wchar_t* key, const wchar_t* name);
};

} // namespace haylen::platform
