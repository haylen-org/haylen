#pragma once

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

#include "haylen/io/PackageReader.hpp"

namespace haylen::io {

// Reads a regular file of the host file system in ranges.
class FileReader final : public PackageReader {
  public:
    explicit FileReader(const std::filesystem::path& file);

    [[nodiscard]] std::uint64_t getSize() const noexcept override {
        return size;
    }
    [[nodiscard]] std::size_t read(std::uint64_t offset, std::span<std::uint8_t> target) override;

  private:
    std::string name;
    std::ifstream stream;
    std::uint64_t size = 0;
    std::mutex mutex;
};

} // namespace haylen::io
