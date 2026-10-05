#pragma once

#include <zip.h>

#include <memory>
#include <string>

#include "haylen/io/Package.hpp"
#include "io/ZipArchive.hpp"

namespace haylen::io {

// Package that reads the entries of a zip archive.
class ZipPackage final : public Package {
  public:
    explicit ZipPackage(std::shared_ptr<ZipArchive> openArchive);

    [[nodiscard]] std::string_view getName() const noexcept override {
        return archive->getName();
    }
    [[nodiscard]] bool exists(std::string_view path) const override;
    [[nodiscard]] std::uint64_t getFileSize(std::string_view path) const override;
    [[nodiscard]] std::unique_ptr<PackageReader> openReader(std::string_view path) const override;
    [[nodiscard]] std::vector<std::string> list(std::string_view directory) const override;

  private:
    // Finds an entry while the caller holds the lock of the archive.
    [[nodiscard]] zip_stat_t find(const std::string& entry) const;

    std::shared_ptr<ZipArchive> archive;
};

} // namespace haylen::io
