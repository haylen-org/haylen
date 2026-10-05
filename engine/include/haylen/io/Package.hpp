#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/io/PackageReader.hpp"

namespace haylen::io {

// Read-only view of an app package: a folder or a zip archive holding `app.json`, the Lua modules under `source` with `source/main.lua` as the entry point, the assets under `content` and the manifest and Lua modules of every plugin under `plugins`. All methods are thread-safe and take normalized relative paths. Files of any size stream through `openReader`, which reads only the ranges it is asked for.
class Package {
  public:
    // Whole reads of larger files throw, because such files stream through a reader instead.
    static constexpr std::uint64_t kMaxReadSize = std::uint64_t{1} << 30;

    virtual ~Package() = default;

    [[nodiscard]] static std::unique_ptr<Package> openDirectory(const std::filesystem::path& root);

    // Opens a zip archive in place and reads its entries when they are asked for.
    [[nodiscard]] static std::unique_ptr<Package> openZip(const std::filesystem::path& file);
    [[nodiscard]] static std::unique_ptr<Package> openZip(std::vector<std::uint8_t> bytes, std::string name);
    [[nodiscard]] static std::unique_ptr<Package> open(const std::filesystem::path& path);

    [[nodiscard]] virtual std::string_view getName() const noexcept = 0;
    [[nodiscard]] virtual bool exists(std::string_view path) const = 0;
    [[nodiscard]] virtual std::uint64_t getFileSize(std::string_view path) const = 0;
    [[nodiscard]] virtual std::unique_ptr<PackageReader> openReader(std::string_view path) const = 0;
    [[nodiscard]] virtual std::vector<std::string> list(std::string_view directory) const = 0;

    // The folder a package reads from, which only folder packages have.
    [[nodiscard]] virtual std::optional<std::filesystem::path> getDirectory() const {
        return std::nullopt;
    }

    // Tells whether a file holds the bytecode of a Lua module that the authenticated catalog of a protected release vouches for, which only the module loader of the engine loads. Every other file of every package loads only as text.
    [[nodiscard]] virtual bool isLuaBytecode(std::string_view path) const;

    [[nodiscard]] std::vector<std::uint8_t> read(std::string_view path) const;

    // Reads at most `size` bytes at an offset, fewer when the file ends first.
    [[nodiscard]] std::vector<std::uint8_t> readRange(std::string_view path, std::uint64_t offset, std::size_t size) const;
    [[nodiscard]] std::string readText(std::string_view path) const;
    [[nodiscard]] bool assetExists(std::string_view path) const;
    [[nodiscard]] std::uint64_t getAssetSize(std::string_view path) const;
    [[nodiscard]] std::vector<std::uint8_t> readAsset(std::string_view path) const;
    [[nodiscard]] std::vector<std::uint8_t> readAssetRange(std::string_view path, std::uint64_t offset, std::size_t size) const;
    [[nodiscard]] std::string readAssetText(std::string_view path) const;
    [[nodiscard]] std::vector<std::string> listAssets(std::string_view directory) const;
};

} // namespace haylen::io
