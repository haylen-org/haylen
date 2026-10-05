#pragma once

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

#include "haylen/io/PackageReader.hpp"

namespace haylen::io {

// Reads a regular file of the host file system in ranges. Its messages name the file by the name it is given, such as its path inside its package.
class FileReader final : public PackageReader {
  public:
    FileReader(const std::filesystem::path& file, std::string fileName);

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
