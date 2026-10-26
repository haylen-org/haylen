#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "content/FolderStoreFiles.hpp"
#include "content/StoreFiles.hpp"

namespace haylen::test {

// The files of a content store in a folder that behave like the store of the web: the bytes that a run appends can be read, while bytes from an earlier run can be read only once `load` brought them in. It counts the ranges it read in, leaving out the ones it held already, as the store of the web does.
class BrowserStoreFiles final : public content::StoreFiles {
  public:
    explicit BrowserStoreFiles(const std::filesystem::path& folder) : files(folder) {}

    // Holds a number of bytes as not written yet, as the worker of the web does while its writes fall behind.
    void setUnwrittenBytes(std::uint64_t value) noexcept {
        unwritten = value;
    }
    [[nodiscard]] std::uint64_t getUnwrittenBytes() const override {
        return unwritten;
    }

    // Lets go of every record that a download appends at once, as the store of the web does past its budget during a large download.
    void dropAppended() noexcept {
        keepsAppended = false;
    }

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> read(const std::string& name) const override {
        return files.read(name);
    }
    void write(const std::string& name, std::span<const std::uint8_t> bytes) override {
        files.write(name, bytes);
    }
    std::uint64_t append(const std::string& name, std::span<const std::uint8_t> bytes) override {
        const std::uint64_t offset = files.append(name, bytes);
        std::scoped_lock lock(mutex);
        if (keepsAppended) {
            resident.emplace(name, offset, bytes.size());
        }
        return offset;
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
    [[nodiscard]] bool isResident(const std::string& name, std::uint64_t offset, std::uint64_t size) const override {
        std::scoped_lock lock(mutex);
        return resident.contains({name, offset, size});
    }
    void load(std::vector<Range> ranges, std::function<void(bool)> done) override {
        {
            std::scoped_lock lock(mutex);
            for (const Range& range : ranges) {
                loaded += resident.emplace(range.name, range.offset, range.size).second ? 1U : 0U;
            }
        }
        done(true);
    }

    [[nodiscard]] std::size_t getLoadedCount() const {
        std::scoped_lock lock(mutex);
        return loaded;
    }

  private:
    content::FolderStoreFiles files;
    mutable std::mutex mutex;
    std::set<std::tuple<std::string, std::uint64_t, std::uint64_t>> resident;
    std::size_t loaded = 0;
    std::atomic<std::uint64_t> unwritten = 0;
    bool keepsAppended = true;
};

} // namespace haylen::test
