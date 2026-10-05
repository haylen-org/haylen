#include "io/FileReader.hpp"

#include <algorithm>
#include <stdexcept>
#include <system_error>

namespace haylen::io {

FileReader::FileReader(const std::filesystem::path& file) : name(file.generic_string()) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(file, error)) {
        throw std::runtime_error("The package file \"" + name + "\" was not found.");
    }

    stream.open(file, std::ios::binary);
    size = std::filesystem::file_size(file, error);
    if (!stream || error) {
        throw std::runtime_error("The package file \"" + name + "\" could not be opened.");
    }
}

std::size_t FileReader::read(std::uint64_t offset, std::span<std::uint8_t> target) {
    if (offset >= size || target.empty()) {
        return 0;
    }

    const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(target.size(), size - offset));
    std::scoped_lock lock(mutex);
    stream.clear();
    stream.seekg(static_cast<std::streamoff>(offset));
    stream.read(reinterpret_cast<char*>(target.data()), static_cast<std::streamsize>(count));
    if (static_cast<std::size_t>(stream.gcount()) != count) {
        throw std::runtime_error("The package file \"" + name + "\" could not be read.");
    }
    return count;
}

} // namespace haylen::io
