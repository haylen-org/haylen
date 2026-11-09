#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "haylen/platform/SystemInfo.hpp"

namespace haylen::platform {

// Reads what the Java side of the Android host tells about the device. It compiles on every platform, so the tests of every host check it.
class AndroidDeviceInfo final {
  public:
    // Takes the JSON of `HaylenActivity.systemInfo`, whose `television` and `tablet` pick the kind of device, and leaves every value it lacks unknown.
    [[nodiscard]] static SystemInfo toSystemInfo(std::string_view json);

    // Takes the JSON array of language tags that `HaylenActivity` reads from the locales of its configuration.
    [[nodiscard]] static std::vector<std::string> toLanguages(std::string_view json);
};

} // namespace haylen::platform
