#pragma once

#include <ctime>

namespace haylen::localization {

// The local time of the device through the C library of the system of the build, which `posix/LocalTime.cpp` and `windows/LocalTime.cpp` reach.
class LocalTime final {
  public:
    // Fills the fields of the moment in the time zone of the device and returns whether the system could express it, which some systems cannot for moments before 1970.
    [[nodiscard]] static bool convert(std::time_t moment, std::tm& local) noexcept;

    // Reads the time zone of the device again, since the C library keeps the zone it read first.
    static void reloadZone() noexcept;
};

} // namespace haylen::localization
