#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "content/StoreFile.hpp"
#include "content/StoreFiles.hpp"

namespace haylen::content {

// The files of a local content store in a folder of the device, readable only by the user that runs the app. It keeps the files it reads and appends to open, so reads of ranges take no lock of their own, and a file that the platform removes while it is open is gone, as the folder shows it.
class FolderStoreFiles final : public StoreFiles {
  public:
    explicit FolderStoreFiles(std::filesystem::path storeFolder);

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> read(const std::string& name) const override;
    void write(const std::string& name, std::span<const std::uint8_t> bytes) override;
    std::uint64_t append(const std::string& name, std::span<const std::uint8_t> bytes) override;
    void sync(const std::string& name) override;
    void truncate(const std::string& name, std::uint64_t size) override;
    void remove(const std::string& name) override;
    [[nodiscard]] std::uint64_t getSize(const std::string& name) const override;
    [[nodiscard]] std::vector<std::string> list(const std::string& folder) const override;
    void readRange(const std::string& name, std::uint64_t offset, std::span<std::uint8_t> target) const override;
    [[nodiscard]] std::uint64_t getAvailableSpace() const override;

    [[nodiscard]] const std::filesystem::path& getFolder() const noexcept {
        return root;
    }

  private:
    [[nodiscard]] std::shared_ptr<StoreFile> open(const std::string& name, bool create) const;
    void forget(const std::string& name);
    void createFolder(const std::filesystem::path& folder) const;

    std::filesystem::path root;
    mutable std::mutex mutex;
    mutable std::map<std::string, std::shared_ptr<StoreFile>> files;
};

} // namespace haylen::content
