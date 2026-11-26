#pragma once

#include <zip.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "haylen/io/Package.hpp"
#include "io/ZipArchive.hpp"

namespace haylen::io {

// Package that reads the entries of a zip archive.
class ZipPackage final : public Package {
  public:
    // Throws `std::runtime_error` for an archive with an entry whose name is not a normalized relative path.
    ZipPackage(std::shared_ptr<ZipArchive> openArchive, std::optional<std::string> packageLocation);

    [[nodiscard]] std::string_view getName() const noexcept override {
        return archive->getName();
    }
    [[nodiscard]] bool exists(std::string_view path) const override;
    [[nodiscard]] std::uint64_t getFileSize(std::string_view path) const override;
    [[nodiscard]] std::unique_ptr<PackageReader> openReader(std::string_view path) const override;
    [[nodiscard]] std::vector<std::string> list(std::string_view directory) const override;
    [[nodiscard]] std::optional<std::string> getLocation() const override {
        return location;
    }

  private:
    void checkEntries() const;
    [[nodiscard]] static bool isNormalized(std::string_view name);

    // Finds an entry while the caller holds the lock of the archive.
    [[nodiscard]] zip_stat_t find(const std::string& entry) const;

    std::shared_ptr<ZipArchive> archive;
    std::optional<std::string> location;
};

} // namespace haylen::io
