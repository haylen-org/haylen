#pragma once

#include <cstdint>
#include <filesystem>

#include "content/FolderStoreFiles.hpp"
#include "content/StoreFiles.hpp"
#include "haylen/content/Error.hpp"

namespace haylen::test {

// The files of a content store in a folder, in a browser whose quota for the site ends after a test's number of appended bytes, while it reports room to spare, as an origin whose quota the browser lowered after it told the page its estimate.
class QuotaStoreFiles final : public content::StoreFiles {
  public:
    QuotaStoreFiles(const std::filesystem::path& folder, std::uint64_t quotaBytes) : files(folder), quota(quotaBytes) {}

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> read(const std::string& name) const override {
        return files.read(name);
    }
    void write(const std::string& name, std::span<const std::uint8_t> bytes) override {
        files.write(name, bytes);
    }
    std::uint64_t append(const std::string& name, std::span<const std::uint8_t> bytes) override {
        if (appended + bytes.size() > quota) {
            throw content::Error(content::Error::Code::QuotaExceeded, "The browser ran out of storage for this site.");
        }
        appended += bytes.size();
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
    std::uint64_t quota;
    std::uint64_t appended = 0;
};

} // namespace haylen::test
