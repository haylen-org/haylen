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

// Read-only view of an app package: a folder or a zip archive holding `haylen.json`, the Lua modules under `source` with `source/main.lua` as the entry point, the assets under `content` and the manifest and Lua modules of every plugin under `plugins`. All methods are thread-safe and take normalized relative paths. Files of any size stream through `openReader`, which reads only the ranges it is asked for.
class Package {
  public:
    // Whole reads of larger files and range reads of longer ranges throw, because such files stream through a reader instead.
    static constexpr std::uint64_t kMaxReadSize = std::uint64_t{1} << 30;

    virtual ~Package() = default;

    // A package on disk takes the resolved path of its folder or file as its location, unless its creator names another one, such as the address that a launched package came from.
    [[nodiscard]] static std::unique_ptr<Package> openDirectory(const std::filesystem::path& root, std::optional<std::string> location = std::nullopt);

    // Opens a zip archive in place and reads its entries when they are asked for. Throws `std::runtime_error` for a file that is not a zip archive or an archive with an entry whose name is not a relative path with forward slashes, such as `./haylen.json`.
    [[nodiscard]] static std::unique_ptr<Package> openZip(const std::filesystem::path& file, std::optional<std::string> location = std::nullopt);

    // Opens a zip archive held in memory, whose location its creator names.
    [[nodiscard]] static std::unique_ptr<Package> openZip(std::vector<std::uint8_t> bytes, std::string name, std::optional<std::string> location = std::nullopt);
    [[nodiscard]] static std::unique_ptr<Package> open(const std::filesystem::path& path, std::optional<std::string> location = std::nullopt);

    [[nodiscard]] virtual std::string_view getName() const noexcept = 0;
    [[nodiscard]] virtual bool exists(std::string_view path) const = 0;
    [[nodiscard]] virtual std::uint64_t getFileSize(std::string_view path) const = 0;
    [[nodiscard]] virtual std::unique_ptr<PackageReader> openReader(std::string_view path) const = 0;
    [[nodiscard]] virtual std::vector<std::string> list(std::string_view directory) const = 0;

    // The folder a package reads from, which only folder packages have.
    [[nodiscard]] virtual std::optional<std::filesystem::path> getDirectory() const {
        return std::nullopt;
    }

    // Where a package comes from as UTF-8 text, which names an app whose `haylen.json` names no identifier: the resolved path of a folder or a zip file on disk, the address that a launched package came from, or the location that the creator of a package in memory names. The bundle of a built app has none, since the tooling writes the identifier into it.
    [[nodiscard]] virtual std::optional<std::string> getLocation() const {
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
    [[nodiscard]] std::unique_ptr<PackageReader> openAssetReader(std::string_view path) const;
    [[nodiscard]] std::vector<std::uint8_t> readAsset(std::string_view path) const;
    [[nodiscard]] std::vector<std::uint8_t> readAssetRange(std::string_view path, std::uint64_t offset, std::size_t size) const;
    [[nodiscard]] std::string readAssetText(std::string_view path) const;
    [[nodiscard]] std::vector<std::string> listAssets(std::string_view directory) const;

  private:
    // The resolved path of a package on disk as UTF-8 text, which names its location.
    [[nodiscard]] static std::string resolvedText(const std::filesystem::path& resolved);
};

} // namespace haylen::io
