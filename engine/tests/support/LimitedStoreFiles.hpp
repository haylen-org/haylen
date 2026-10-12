#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>

#include "content/FolderStoreFiles.hpp"
#include "content/StoreFiles.hpp"

namespace haylen::test {

// The files of a content store in a folder, on a device that reports only as much free room as a test gives it.
class LimitedStoreFiles final : public content::StoreFiles {
  public:
    LimitedStoreFiles(const std::filesystem::path& folder, std::uint64_t availableBytes) : files(folder), available(availableBytes) {}

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> read(const std::string& name) const override {
        return files.read(name);
    }
    void write(const std::string& name, std::span<const std::uint8_t> bytes) override {
        files.write(name, bytes);
    }
    std::uint64_t append(const std::string& name, std::span<const std::uint8_t> bytes) override {
        return files.append(name, bytes);
    }
    void sync(const std::string& name) override {
        files.sync(name);
    }
    void truncate(const std::string& name, std::uint64_t size) override {
        files.truncate(name, size);
    }
    void remove(const std::string& name) override {
        files.remove(name);
    }
    [[nodiscard]] std::uint64_t getSize(const std::string& name) const override {
        return files.getSize(name);
    }
    [[nodiscard]] std::vector<std::string> list(const std::string& folder) const override {
        return files.list(folder);
    }
    void readRange(const std::string& name, std::uint64_t offset, std::span<std::uint8_t> target) const override {
        files.readRange(name, offset, target);
    }
    [[nodiscard]] std::uint64_t getAvailableSpace() const override {
        return available;
    }

  private:
    content::FolderStoreFiles files;
    std::uint64_t available;
};

} // namespace haylen::test
