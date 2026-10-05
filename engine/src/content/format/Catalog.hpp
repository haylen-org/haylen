#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "content/Delivery.hpp"
#include "content/crypto/Digest.hpp"
#include "content/format/Compression.hpp"

namespace haylen::content {

// The decrypted catalog of a manifest: the files of the virtual package with their paths, sizes and chunks, kept as the compact binary tables it arrived in. Files are sorted by path, so a lookup is a binary search and a folder is one range of the table. The catalog is the only place that names paths, and its bytes are wiped when it ends.
class Catalog final {
  public:
    static constexpr std::uint16_t kVersion = 1;
    static constexpr std::size_t kHeaderSize = 64;
    static constexpr std::size_t kFileSize = 40;
    static constexpr std::size_t kChunkSize = 88;
    static constexpr std::size_t kPartSize = 16;
    static constexpr std::size_t kMaximumPathLength = 1024;
    static constexpr std::array<std::uint8_t, 4> kMagic = {'H', 'C', 'A', 'T'};

    // What a file holds: plain data, or the bytecode of a Lua module, which only the app domain holds and only the module loader of the engine loads.
    enum class Kind : std::uint8_t {
        File = 0,
        LuaBytecode = 1,
    };

    struct File {
        std::string_view path;
        Kind kind = Kind::File;
        Delivery delivery = Delivery::Required;
        std::uint64_t size = 0;
        std::uint64_t firstPart = 0;
        std::uint64_t partCount = 0;
    };

    // A stored chunk, listed once however many files use it. The shard is its position in the shard list of the manifest.
    struct Chunk {
        Digest storedId;
        Digest contentId;
        std::uint64_t plainSize = 0;
        std::uint64_t encodedSize = 0;
        std::uint32_t shard = 0;
        Compression::Codec codec = Compression::Codec::None;
        std::uint8_t profile = Compression::kNoneProfile;
    };

    // One chunk of a file in order, with the offset in the file where it starts.
    struct Part {
        std::uint64_t chunk = 0;
        std::uint64_t offset = 0;
    };

    Catalog(Catalog&& other) noexcept = default;
    ~Catalog();

    Catalog(const Catalog&) = delete;
    Catalog& operator=(const Catalog&) = delete;
    Catalog& operator=(Catalog&&) = delete;

    // Checks every table of a plain catalog for a manifest with the given number of shards, and throws `UnsupportedFormat`, `UnsupportedVersion` or `CorruptCatalog` for anything else: sizes that do not add up, unsorted or repeated paths and IDs, paths that are not normalized UTF-8, chunks of impossible sizes or of missing shards, and files whose parts do not cover them exactly.
    [[nodiscard]] static Catalog parse(std::vector<std::uint8_t> plain, std::uint64_t shardCount);

    // Tells whether a path may name a file of a catalog: normalized, valid UTF-8 without control characters and no longer than the limit.
    [[nodiscard]] static bool isValidPath(std::string_view path);

    [[nodiscard]] std::uint64_t getFileCount() const noexcept {
        return fileCount;
    }
    [[nodiscard]] std::uint64_t getChunkCount() const noexcept {
        return chunkCount;
    }
    [[nodiscard]] std::uint64_t getPartCount() const noexcept {
        return partCount;
    }
    [[nodiscard]] std::size_t getByteSize() const noexcept {
        return bytes.size();
    }

    [[nodiscard]] File getFile(std::uint64_t index) const noexcept;
    [[nodiscard]] Chunk getChunk(std::uint64_t index) const noexcept;
    [[nodiscard]] Part getPart(std::uint64_t index) const noexcept;

    [[nodiscard]] std::optional<std::uint64_t> findFile(std::string_view path) const noexcept;

    // Returns the range of files under a folder, every file for the empty root folder.
    [[nodiscard]] std::pair<std::uint64_t, std::uint64_t> findFolder(std::string_view folder) const;

    // Returns the part of a file that holds an offset inside it.
    [[nodiscard]] std::uint64_t findPart(const File& file, std::uint64_t offset) const noexcept;

  private:
    // The offsets of the fields inside a row of each table.
    struct FileLayout {
        static constexpr std::size_t kPathOffset = 0;
        static constexpr std::size_t kPathLength = 8;
        static constexpr std::size_t kKind = 12;
        static constexpr std::size_t kDelivery = 13;
        static constexpr std::size_t kReserved = 14;
        static constexpr std::size_t kSize = 16;
        static constexpr std::size_t kFirstPart = 24;
        static constexpr std::size_t kPartCount = 32;
    };
    struct ChunkLayout {
        static constexpr std::size_t kStoredId = 0;
        static constexpr std::size_t kContentId = 32;
        static constexpr std::size_t kPlainSize = 64;
        static constexpr std::size_t kEncodedSize = 72;
        static constexpr std::size_t kShard = 80;
        static constexpr std::size_t kCodec = 84;
        static constexpr std::size_t kProfile = 85;
        static constexpr std::size_t kReserved = 86;
    };
    struct PartLayout {
        static constexpr std::size_t kChunk = 0;
        static constexpr std::size_t kOffset = 8;
    };

    explicit Catalog(std::vector<std::uint8_t> plain) noexcept;

    template <std::unsigned_integral T> [[nodiscard]] T load(std::size_t offset) const noexcept;

    [[nodiscard]] std::string_view getPath(std::uint64_t index) const noexcept;
    [[nodiscard]] std::uint64_t lowerBound(std::string_view path) const noexcept;
    void validate(std::uint64_t shardCount) const;

    std::vector<std::uint8_t> bytes;
    std::uint64_t fileCount = 0;
    std::uint64_t chunkCount = 0;
    std::uint64_t partCount = 0;
    std::uint64_t stringSize = 0;
    std::size_t chunkTable = 0;
    std::size_t partTable = 0;
    std::size_t stringTable = 0;
};

} // namespace haylen::content
