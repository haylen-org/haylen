#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/io/Package.hpp"

namespace haylen::io {

// Package held in memory whose files can be replaced while the app runs, used by editors and hot reload.
class MemoryPackage final : public Package {
  public:
    explicit MemoryPackage(std::string packageName, std::map<std::string, std::vector<std::uint8_t>> contents = {});

    void setFile(std::string_view path, std::vector<std::uint8_t> bytes);
    bool removeFile(std::string_view path);

    [[nodiscard]] std::string_view getName() const noexcept override {
        return name;
    }
    [[nodiscard]] bool exists(std::string_view path) const override;
    [[nodiscard]] std::vector<std::uint8_t> read(std::string_view path) const override;
    [[nodiscard]] std::vector<std::string> list(std::string_view directory) const override;

  private:
    std::string name;
    mutable std::mutex mutex;
    std::map<std::string, std::vector<std::uint8_t>, std::less<>> files;
};

} // namespace haylen::io
