#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace haylen::io {

// Read-only view of an app package: a folder or a zip archive holding app.json, the Lua modules under source with source/main.lua as the entry point, the assets under content and the manifest and Lua modules of every plugin under plugins. All methods are thread-safe and take normalized relative paths.
class Package {
  public:
    virtual ~Package() = default;

    [[nodiscard]] static std::unique_ptr<Package> openDirectory(const std::filesystem::path& root);
    [[nodiscard]] static std::unique_ptr<Package> openZip(const std::filesystem::path& file);
    [[nodiscard]] static std::unique_ptr<Package> openZip(std::vector<std::uint8_t> bytes, std::string name);
    [[nodiscard]] static std::unique_ptr<Package> open(const std::filesystem::path& path);

    [[nodiscard]] virtual std::string_view getName() const noexcept = 0;
    [[nodiscard]] virtual bool exists(std::string_view path) const = 0;
    [[nodiscard]] virtual std::vector<std::uint8_t> read(std::string_view path) const = 0;
    [[nodiscard]] virtual std::vector<std::string> list(std::string_view directory) const = 0;

    // The folder a package reads from, which only folder packages have.
    [[nodiscard]] virtual std::optional<std::filesystem::path> getDirectory() const {
        return std::nullopt;
    }

    [[nodiscard]] std::string readText(std::string_view path) const;
    [[nodiscard]] bool assetExists(std::string_view path) const;
    [[nodiscard]] std::vector<std::uint8_t> readAsset(std::string_view path) const;
    [[nodiscard]] std::string readAssetText(std::string_view path) const;
    [[nodiscard]] std::vector<std::string> listAssets(std::string_view directory) const;

  protected:
    // Reads a whole regular file of the host file system.
    [[nodiscard]] static std::vector<std::uint8_t> readFile(const std::filesystem::path& file);
};

} // namespace haylen::io
