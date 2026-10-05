#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/io/Package.hpp"

namespace haylen::io {

// A package whose files can be replaced and removed in memory over a read-only base package, such as a zip file, the assets of an APK or the bundle of an app, for the files that a page or the development server sends while the app runs. A reader keeps reading the bytes the file had when it opened.
class OverlayPackage final : public Package {
  public:
    explicit OverlayPackage(std::shared_ptr<const Package> basePackage);

    void setFile(std::string_view path, std::vector<std::uint8_t> bytes);

    // Hides a file, of the overlay or of the base, and returns whether one existed.
    bool removeFile(std::string_view path);

    [[nodiscard]] std::string_view getName() const noexcept override {
        return base->getName();
    }
    [[nodiscard]] bool exists(std::string_view path) const override;
    [[nodiscard]] std::uint64_t getFileSize(std::string_view path) const override;
    [[nodiscard]] std::unique_ptr<PackageReader> openReader(std::string_view path) const override;
    [[nodiscard]] std::vector<std::string> list(std::string_view directory) const override;
    [[nodiscard]] std::optional<std::filesystem::path> getDirectory() const override {
        return base->getDirectory();
    }
    [[nodiscard]] bool isLuaBytecode(std::string_view path) const override;

  private:
    using Bytes = std::shared_ptr<const std::vector<std::uint8_t>>;

    // Returns the bytes of a file of the overlay, or an empty optional when the overlay leaves the path to the base. A removed file holds null bytes.
    [[nodiscard]] std::optional<Bytes> findOwn(std::string_view path) const;

    std::shared_ptr<const Package> base;
    mutable std::mutex mutex;
    std::map<std::string, Bytes, std::less<>> files;
};

} // namespace haylen::io
