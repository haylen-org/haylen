#pragma once

#include <zip.h>

#include <mutex>
#include <string>

#include "haylen/io/Package.hpp"

namespace haylen::io {

// Package that reads a zip archive kept in memory.
class ZipPackage final : public Package {
  public:
    ZipPackage(zip_t* openArchive, std::vector<std::uint8_t> archiveBytes, std::string archiveName);
    ~ZipPackage() override;

    ZipPackage(const ZipPackage&) = delete;
    ZipPackage& operator=(const ZipPackage&) = delete;

    [[nodiscard]] std::string_view getName() const noexcept override {
        return name;
    }
    [[nodiscard]] bool exists(std::string_view path) const override;
    [[nodiscard]] std::vector<std::uint8_t> read(std::string_view path) const override;
    [[nodiscard]] std::vector<std::string> list(std::string_view directory) const override;

  private:
    zip_t* archive;
    std::vector<std::uint8_t> bytes;
    std::string name;
    mutable std::mutex mutex;
};

} // namespace haylen::io
