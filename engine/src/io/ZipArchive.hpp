#pragma once

#include <zip.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace haylen::io {

// An open zip archive that a package and its readers share, read either from its file or from bytes it keeps in memory. libzip serves one caller at a time, so every call goes through the lock.
class ZipArchive final {
  public:
    ZipArchive(zip_t* openArchive, std::vector<std::uint8_t> archiveBytes, std::string archiveName);
    ~ZipArchive();

    ZipArchive(const ZipArchive&) = delete;
    ZipArchive& operator=(const ZipArchive&) = delete;

    [[nodiscard]] zip_t* get() const noexcept {
        return archive;
    }
    [[nodiscard]] std::mutex& getMutex() const noexcept {
        return mutex;
    }
    [[nodiscard]] const std::string& getName() const noexcept {
        return name;
    }

  private:
    zip_t* archive;
    std::vector<std::uint8_t> bytes;
    std::string name;
    mutable std::mutex mutex;
};

} // namespace haylen::io
