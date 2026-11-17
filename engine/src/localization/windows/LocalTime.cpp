#include "localization/LocalTime.hpp"

namespace haylen::localization {

bool LocalTime::convert(std::time_t moment, std::tm& local) noexcept {
    return localtime_s(&local, &moment) == 0;
}

void LocalTime::reloadZone() noexcept {
    _tzset();
}

} // namespace haylen::localization
