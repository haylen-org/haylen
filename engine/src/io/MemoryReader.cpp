#include "io/MemoryReader.hpp"

#include <algorithm>
#include <utility>

namespace haylen::io {

MemoryReader::MemoryReader(std::shared_ptr<const std::vector<std::uint8_t>> content) : bytes(std::move(content)) {}

std::size_t MemoryReader::read(std::uint64_t offset, std::span<std::uint8_t> target) {
    if (offset >= bytes->size()) {
        return 0;
    }
    const auto start = static_cast<std::size_t>(offset);
    const std::size_t count = std::min(target.size(), bytes->size() - start);
    std::copy_n(bytes->begin() + static_cast<std::ptrdiff_t>(start), count, target.begin());
    return count;
}

} // namespace haylen::io
