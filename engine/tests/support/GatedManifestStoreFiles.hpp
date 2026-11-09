#pragma once

#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>

#include "content/FolderStoreFiles.hpp"
#include "content/StoreFiles.hpp"

namespace haylen::test {

// The files of a content store in a folder, whose reads of manifests wait while a test holds the gate closed, as a device that is slow to read what a collection looks at.
class GatedManifestStoreFiles final : public content::StoreFiles {
  public:
    explicit GatedManifestStoreFiles(const std::filesystem::path& folder) : files(folder) {}

    void close() {
        std::scoped_lock lock(mutex);
        closed = true;
    }

    void open() {
        {
            std::scoped_lock lock(mutex);
            closed = false;
        }
        changed.notify_all();
    }

    [[nodiscard]] bool isHolding() const {
        std::scoped_lock lock(mutex);
        return held > 0;
    }

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> read(const std::string& name) const override {
        if (name.starts_with("manifests/")) {
            std::unique_lock lock(mutex);
            ++held;
            changed.wait(lock, [this] { return !closed; });
            --held;
        }
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
        return files.getAvailableSpace();
    }

  private:
    content::FolderStoreFiles files;
    mutable std::mutex mutex;
    mutable std::condition_variable changed;
    bool closed = false;
    mutable int held = 0;
};

} // namespace haylen::test
