#pragma once

#include <filesystem>
#include <string>

#include "haylen/io/Package.hpp"

namespace haylen::io {

// Package that reads the files of a folder.
class DirectoryPackage final : public Package {
  public:
    DirectoryPackage(std::filesystem::path folder, std::string packageLocation);

    [[nodiscard]] std::string_view getName() const noexcept override {
        return name;
    }
    [[nodiscard]] bool exists(std::string_view path) const override;
    [[nodiscard]] std::uint64_t getFileSize(std::string_view path) const override;
    [[nodiscard]] std::unique_ptr<PackageReader> openReader(std::string_view path) const override;
    [[nodiscard]] std::vector<std::string> list(std::string_view directory) const override;
    [[nodiscard]] std::optional<std::filesystem::path> getDirectory() const override {
        return root;
    }
    [[nodiscard]] std::optional<std::string> getLocation() const override {
        return location;
    }

  private:
    std::filesystem::path root;
    std::string name;
    std::string location;
};

} // namespace haylen::io
