#include "platform/android/AndroidGlesVersion.hpp"

#include <charconv>
#include <cstdint>

#if defined(__ANDROID__)
#include <sys/system_properties.h>

#include <array>
#include <cstddef>
#endif

namespace haylen::platform {

AndroidGlesVersion AndroidGlesVersion::fromProperty(std::string_view value) {
    std::uint32_t declared = 0;
    std::from_chars(value.data(), value.data() + value.size(), declared);
    return {.major = static_cast<int>(declared >> 16U), .minor = static_cast<int>(declared & 0xFFFFU)};
}

#if defined(__ANDROID__)
AndroidGlesVersion AndroidGlesVersion::read() {
    std::array<char, PROP_VALUE_MAX> value{};
    const int length = __system_property_get("ro.opengles.version", value.data());
    return fromProperty(std::string_view(value.data(), static_cast<std::size_t>(length)));
}
#endif

} // namespace haylen::platform
