#include "platform/linux/LinuxFilePattern.hpp"

#include <cctype>

namespace haylen::platform {

std::string LinuxFilePattern::fromExtension(std::string_view extension) {
    std::string pattern = "*.";
    for (const char character : extension) {
        const auto code = static_cast<unsigned char>(character);
        if (code < 128 && std::isalpha(code) != 0) {
            pattern += {'[', static_cast<char>(std::tolower(code)), static_cast<char>(std::toupper(code)), ']'};
        } else {
            pattern += character;
        }
    }
    return pattern;
}

} // namespace haylen::platform
