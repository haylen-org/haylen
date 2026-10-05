#include "support/CountingPackage.hpp"

#include <utility>

namespace haylen::test {

CountingPackage::CountingPackage(std::shared_ptr<const io::Package> innerPackage) : inner(std::move(innerPackage)) {}

std::unique_ptr<io::PackageReader> CountingPackage::openReader(std::string_view path) const {
    return std::make_unique<Reader>(inner->openReader(path), counters);
}

void CountingPackage::reset() noexcept {
    counters->bytes = 0;
    counters->largest = 0;
}

CountingPackage::Reader::Reader(std::unique_ptr<io::PackageReader> innerReader, std::shared_ptr<Counters> sharedCounters) : inner(std::move(innerReader)), counters(std::move(sharedCounters)) {}

std::size_t CountingPackage::Reader::read(std::uint64_t offset, std::span<std::uint8_t> target) {
    const std::size_t count = inner->read(offset, target);
    counters->bytes += count;
    std::uint64_t largest = counters->largest;
    while (count > largest && !counters->largest.compare_exchange_weak(largest, count)) {}
    return count;
}

} // namespace haylen::test
