#pragma once

#include <chrono>
#include <functional>
#include <string>

#include "haylen/platform/SystemInfo.hpp"

namespace haylen::platform {

// The system of Linux as the engine sees it: what the device is, the battery and opening urls with the app that handles them.
class LinuxSystem final {
  public:
    [[nodiscard]] static SystemInfo getInfo();
    static void openUrl(const std::string& url, std::function<void(bool opened)> callback);

    // Reports the battery now and then every 30 seconds from a thread of its own, since Linux announces no change and some batteries take a while to read.
    static void watchBattery();

  private:
    static constexpr std::chrono::seconds kBatteryInterval{30};
    static constexpr const char* kPowerSupplies = "/sys/class/power_supply";

    [[nodiscard]] static std::string getSystemVersion();
    [[nodiscard]] static std::string getVariable(const char* name);
};

} // namespace haylen::platform
