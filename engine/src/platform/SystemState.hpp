#pragma once

#include <mutex>

#include "haylen/platform/Battery.hpp"
#include "haylen/platform/Theme.hpp"

namespace haylen::platform {

// The theme and the battery of the device as the platform last reported them. Platform services report them from any thread whenever the system tells them of a change, and the host reads them on the frame thread once per frame.
class SystemState final {
  public:
    void setTheme(Theme value);
    [[nodiscard]] Theme getTheme() const;
    void setBattery(const Battery& value);
    [[nodiscard]] Battery getBattery() const;

  private:
    mutable std::mutex mutex;
    Theme theme = Theme::Light;
    Battery battery;
};

} // namespace haylen::platform
