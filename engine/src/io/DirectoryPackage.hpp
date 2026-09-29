#pragma once

#include <filesystem>
#include <string>

#include "haylen/io/Package.hpp"

namespace haylen::io {

// Package that reads the files of a folder.
class DirectoryPackage final : public Package {
  public:
    explicit DirectoryPackage(std::filesystem::path folder);

    [[nodiscard]] std::string_view getName() const noexcept override {
        return name;
    }
    [[nodiscard]] bool exists(std::string_view path) const override;
    [[nodiscard]] std::vector<std::uint8_t> read(std::string_view path) const override;
    [[nodiscard]] std::vector<std::string> list(std::string_view directory) const override;
    [[nodiscard]] std::optional<std::filesystem::path> getDirectory() const override {
        return root;
    }

  private:
    std::filesystem::path root;
    std::string name;
};

} // namespace haylen::io
