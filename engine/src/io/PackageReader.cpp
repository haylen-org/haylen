#include "haylen/io/PackageReader.hpp"

#include <stdexcept>

namespace haylen::io {

void PackageReader::readExactly(std::uint64_t offset, std::span<std::uint8_t> target) {
    std::size_t filled = 0;
    while (filled < target.size()) {
        const std::size_t count = read(offset + filled, target.subspan(filled));
        if (count == 0) {
            throw std::runtime_error("A file of the package ended before the bytes the app reads, so the package is damaged or changed while the app read it.");
        }

        filled += count;
    }
}

} // namespace haylen::io
