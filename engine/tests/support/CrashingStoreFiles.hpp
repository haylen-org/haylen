#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "content/FolderStoreFiles.hpp"
#include "content/StoreFiles.hpp"
#include "haylen/content/Error.hpp"

namespace haylen::test {

// The files of a content store in a folder, on a device that loses power at a moment a test chooses: the write or append that it interrupts reaches the disk only in part, and every change after it fails, as if the app had stopped there. A store opened again on the same folder sees what a device finds after such a loss.
class CrashingStoreFiles final : public content::StoreFiles {
  public:
    explicit CrashingStoreFiles(const std::filesystem::path& folder) : files(folder) {}

    // Loses power during the write of a file whose name starts with the prefix, after that many such writes went through.
    void crashOnWrite(std::string prefix, int after = 0) {
        writePrefix = std::move(prefix);
        writesLeft = after;
    }

    // Loses power during an append, after that many appends went through.
    void crashOnAppend(int after) {
        appendsLeft = after;
    }

    [[nodiscard]] bool hasCrashed() const noexcept {
        return crashed;
    }

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> read(const std::string& name) const override {
        return files.read(name);
    }
    void write(const std::string& name, std::span<const std::uint8_t> bytes) override {
        refuseAfterCrash();
        if (writePrefix && name.starts_with(*writePrefix) && writesLeft-- == 0) {
            files.write(name, bytes.first(bytes.size() / 2));
            crash();
        }
        files.write(name, bytes);
    }
    std::uint64_t append(const std::string& name, std::span<const std::uint8_t> bytes) override {
        refuseAfterCrash();
        if (appendsLeft && (*appendsLeft)-- == 0) {
            files.append(name, bytes.first(bytes.size() / 2));
            crash();
        }
        return files.append(name, bytes);
    }
    void sync(const std::string& name) override {
        refuseAfterCrash();
        files.sync(name);
    }
    void truncate(const std::string& name, std::uint64_t size) override {
        refuseAfterCrash();
        files.truncate(name, size);
    }
    void remove(const std::string& name) override {
        refuseAfterCrash();
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
    [[noreturn]] void crash() {
        crashed = true;
        throw content::Error(content::Error::Code::DiskFull, "The device lost power.");
    }
    void refuseAfterCrash() const {
        if (crashed) {
            throw content::Error(content::Error::Code::DiskFull, "The device lost power.");
        }
    }

    content::FolderStoreFiles files;
    std::optional<std::string> writePrefix;
    int writesLeft = 0;
    std::optional<int> appendsLeft;
    bool crashed = false;
};

} // namespace haylen::test
