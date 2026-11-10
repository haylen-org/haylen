#include "platform/SystemFontReader.hpp"

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace haylen::platform {

std::vector<std::uint8_t> SystemFontReader::read(const std::filesystem::path& file) {
    std::ifstream stream(file, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("The system font \"" + file.string() + "\" cannot be read.");
    }
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

} // namespace haylen::platform
