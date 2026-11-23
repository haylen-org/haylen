#pragma once

#include <memory>
#include <vector>

#include "haylen/io/PackageReader.hpp"

namespace haylen::io {

// Reads bytes held in memory, which it keeps alive.
class MemoryReader final : public PackageReader {
  public:
    explicit MemoryReader(std::shared_ptr<const std::vector<std::uint8_t>> content);

    [[nodiscard]] std::uint64_t getSize() const noexcept override {
        return bytes->size();
    }
    [[nodiscard]] std::size_t read(std::uint64_t offset, std::span<std::uint8_t> target) override;

  private:
    std::shared_ptr<const std::vector<std::uint8_t>> bytes;
};

} // namespace haylen::io
