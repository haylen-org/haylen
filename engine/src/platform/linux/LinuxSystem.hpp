#pragma once

#include <chrono>
#include <functional>
#include <string>

#include "haylen/platform/Permission.hpp"
#include "haylen/platform/SystemInfo.hpp"

namespace haylen::platform {

// The system of Linux as the engine sees it: what the device is, the battery, the time zone and opening urls with the app that handles them.
class LinuxSystem final {
  public:
    [[nodiscard]] static SystemInfo getInfo();
    static void openUrl(const std::string& url, std::function<void(Permission::Answer answer)> callback);

    // Reports the battery now and then every 30 seconds from a thread of its own, since Linux announces no change and some batteries take a while to read.
    static void watchBattery();

    // Reports the time zone of `TZ`, or else the one that `/etc/localtime` links to. A zone that `TZ` names stays for the process.
    static void reportTimeZone();
    [[nodiscard]] static bool isTimeZoneFixed();

    static constexpr const char* kLocalTime = "/etc/localtime";

  private:
    static constexpr std::chrono::seconds kBatteryInterval{30};
    static constexpr const char* kPowerSupplies = "/sys/class/power_supply";

    [[nodiscard]] static std::string getSystemVersion();
    [[nodiscard]] static std::string getVariable(const char* name);
};

} // namespace haylen::platform
