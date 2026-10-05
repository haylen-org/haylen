#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace haylen::io {

// Random access to one file of a package, which reads only the bytes it is asked for, so files of any size stream in bounded memory. Several threads may read through one reader at once.
class PackageReader {
  public:
    virtual ~PackageReader() = default;

    [[nodiscard]] virtual std::uint64_t getSize() const noexcept = 0;

    // Reads the bytes at an offset into the target and returns how many it read, which is fewer than the target holds only at the end of the file.
    [[nodiscard]] virtual std::size_t read(std::uint64_t offset, std::span<std::uint8_t> target) = 0;

    // Fills the whole target from an offset, and throws `std::runtime_error` when the file ends first.
    void readExactly(std::uint64_t offset, std::span<std::uint8_t> target);
};

} // namespace haylen::io
