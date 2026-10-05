#pragma once

#include <zip.h>

#include <memory>
#include <string>

#include "haylen/io/PackageReader.hpp"
#include "io/ZipArchive.hpp"

namespace haylen::io {

// Reads one entry of a zip archive. Entries stored without compression seek directly, and compressed entries decompress forward from the start again when a read goes back.
class ZipReader final : public PackageReader {
  public:
    // The caller holds the lock of the archive.
    ZipReader(std::shared_ptr<ZipArchive> openArchive, zip_uint64_t entryIndex, std::uint64_t entrySize, std::string entryName);
    ~ZipReader() override;

    ZipReader(const ZipReader&) = delete;
    ZipReader& operator=(const ZipReader&) = delete;

    [[nodiscard]] std::uint64_t getSize() const noexcept override {
        return size;
    }
    [[nodiscard]] std::size_t read(std::uint64_t offset, std::span<std::uint8_t> target) override;

  private:
    static constexpr std::size_t kSkipBufferSize = 64 * 1024;

    // Moves the entry to an offset while the caller holds the lock.
    void moveTo(std::uint64_t offset);
    void open();
    [[noreturn]] void fail() const;

    std::shared_ptr<ZipArchive> archive;
    zip_uint64_t index;
    std::uint64_t size;
    std::string name;
    zip_file_t* file = nullptr;
    std::uint64_t position = 0;
};

} // namespace haylen::io
