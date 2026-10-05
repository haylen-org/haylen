#include "haylen/io/PackageReader.hpp"

#include <format>
#include <stdexcept>

namespace haylen::io {

void PackageReader::readExactly(std::uint64_t offset, std::span<std::uint8_t> target) {
    std::size_t filled = 0;
    while (filled < target.size()) {
        const std::size_t count = read(offset + filled, target.subspan(filled));
        if (count == 0) {
            throw std::runtime_error(std::format("A package file of {} bytes ended before the {} bytes asked for at offset {}.", getSize(), target.size(), offset));
        }
        filled += count;
    }
}

} // namespace haylen::io
