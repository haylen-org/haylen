#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include "content/ShardSet.hpp"
#include "content/format/Catalog.hpp"
#include "haylen/io/PackageReader.hpp"

namespace haylen::content {

// Reads one file of an HPAK package in ranges. It finds the chunks that cover a range in the catalog, decrypts only those, and keeps the last chunk it decrypted, so sequential reads decrypt each chunk once. The kept chunk is wiped when the reader ends.
class HpakFileReader final : public io::PackageReader {
  public:
    HpakFileReader(std::shared_ptr<const Catalog> fileCatalog, std::shared_ptr<const ShardSet> fileShards, const Catalog::File& catalogFile);
    ~HpakFileReader() override;

    HpakFileReader(const HpakFileReader&) = delete;
    HpakFileReader& operator=(const HpakFileReader&) = delete;

    [[nodiscard]] std::uint64_t getSize() const noexcept override {
        return file.size;
    }
    [[nodiscard]] std::size_t read(std::uint64_t offset, std::span<std::uint8_t> target) override;

  private:
    std::shared_ptr<const Catalog> catalog;
    std::shared_ptr<const ShardSet> shards;
    Catalog::File file;
    std::mutex mutex;
    std::vector<std::uint8_t> scratch;
    std::vector<std::uint8_t> plain;
    std::optional<std::uint64_t> cachedPart;
};

} // namespace haylen::content
