#include "localization/LocalTime.hpp"

namespace haylen::localization {

bool LocalTime::convert(std::time_t moment, std::tm& local) noexcept {
    return localtime_r(&moment, &local) != nullptr;
}

void LocalTime::reloadZone() noexcept {
    tzset();
}

} // namespace haylen::localization
