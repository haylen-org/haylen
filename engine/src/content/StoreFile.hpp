#pragma once

#include <cstdint>
#include <filesystem>
#include <span>

namespace haylen::content {

// One open file of a local content store, read at any offset by several threads at once without a lock, appended to, and made durable with the call of the system that flushes it to the disk.
class StoreFile final {
  public:
    enum class Mode : std::uint8_t {
        Read,
        Append,
        Replace,
    };

    // Opens a file to read, creates it or opens it to append, or creates it empty to write it whole. Throws `content::Error` with the code of the failure: `PermissionDenied`, `DiskFull` or `CacheCorrupt`.
    StoreFile(const std::filesystem::path& file, Mode mode);
    ~StoreFile();

    StoreFile(const StoreFile&) = delete;
    StoreFile& operator=(const StoreFile&) = delete;

    // Fills the whole target from an offset, and throws `CacheCorrupt` when the file ends first.
    void readAt(std::uint64_t offset, std::span<std::uint8_t> target) const;

    // Writes the bytes at the end of the file and returns the offset where they start.
    std::uint64_t append(std::span<const std::uint8_t> bytes);

    void sync();
    void truncate(std::uint64_t size);
    [[nodiscard]] std::uint64_t getSize() const;

    // Whether the platform removed the file from its folder while it was open, which keeps its bytes for this process alone until it closes.
    [[nodiscard]] bool isRemoved() const;

    // Makes the entries of a folder durable, so a file created in it survives a loss of power. Windows keeps them with the files.
    static void syncFolder(const std::filesystem::path& folder);

  private:
    [[noreturn]] void fail(const char* action) const;

    std::filesystem::path path;
#if defined(_WIN32)
    void* handle = nullptr;
#else
    int descriptor = -1;
#endif
};

} // namespace haylen::content
