#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/io/Package.hpp"

namespace haylen::io {

// Package held in memory whose files can be replaced while the app runs, used by editors and hot reload. A reader keeps reading the bytes the file had when it opened. Its creator names its location, which names an app whose `haylen.json` names no identifier.
class MemoryPackage final : public Package {
  public:
    explicit MemoryPackage(std::string packageName, std::map<std::string, std::vector<std::uint8_t>> contents = {}, std::optional<std::string> packageLocation = std::nullopt);

    void setFile(std::string_view path, std::vector<std::uint8_t> bytes);
    bool removeFile(std::string_view path);

    [[nodiscard]] std::string_view getName() const noexcept override {
        return name;
    }
    [[nodiscard]] bool exists(std::string_view path) const override;
    [[nodiscard]] std::uint64_t getFileSize(std::string_view path) const override;
    [[nodiscard]] std::unique_ptr<PackageReader> openReader(std::string_view path) const override;
    [[nodiscard]] std::vector<std::string> list(std::string_view directory) const override;
    [[nodiscard]] std::optional<std::string> getLocation() const override {
        return location;
    }

  private:
    using Bytes = std::shared_ptr<const std::vector<std::uint8_t>>;

    [[nodiscard]] Bytes find(std::string_view path) const;

    std::string name;
    std::optional<std::string> location;
    mutable std::mutex mutex;
    std::map<std::string, Bytes, std::less<>> files;
};

} // namespace haylen::io
