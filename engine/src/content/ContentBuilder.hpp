#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "content/Delivery.hpp"
#include "content/RecordCache.hpp"
#include "content/ShardWriter.hpp"
#include "content/crypto/ContentKey.hpp"
#include "content/crypto/Digest.hpp"
#include "content/format/Catalog.hpp"
#include "content/format/ShardReference.hpp"
#include "haylen/io/Package.hpp"

namespace haylen::content {

// Builds the shards and the plain catalog of one domain from the files of a source package. Files are taken in path order and read in windows, so no file is ever loaded whole, and each is split into chunks that are identified, compressed and sealed. A chunk whose content an earlier release already stored is referenced in its old shard instead of being stored again, so an update writes only new chunks into new shards and never touches the old ones. An earlier shard stays only while the files still use at least half of its bytes, and the chunks they use of a shard that falls below that move into the new shards, so releases that follow each other never carry more dead bytes than live ones. New shards close near the target size, and a file small against that target stays in one shard.
class ContentBuilder final {
  public:
    static constexpr std::uint64_t kDefaultShardTarget = std::uint64_t{1} << 30;

    struct Input {
        std::string path;
        Delivery delivery = Delivery::Required;
    };

    struct Statistics {
        std::uint64_t newChunks = 0;
        std::uint64_t newBytes = 0;
        std::uint64_t storedBytes = 0;
        std::uint64_t cachedChunks = 0;
        std::uint64_t reusedChunks = 0;
        std::uint64_t reusedBytes = 0;
        std::uint64_t droppedShards = 0;
    };

    struct Result {
        std::vector<std::uint8_t> catalog;

        // The shards of the catalog: the earlier shards the build keeps, in their order, followed by the new ones.
        std::vector<ShardReference> shards;
        std::size_t keptShards = 0;
        Statistics statistics;
    };

    // The cache, when given, holds sealed records of earlier builds under the same key, which save sealing their chunks again.
    ContentBuilder(std::filesystem::path outputFolder, std::shared_ptr<const ContentKey> contentKey, std::uint64_t shardTarget = kDefaultShardTarget, std::shared_ptr<const RecordCache> recordCache = nullptr);

    // Lets the build reference the chunks of an earlier release of the same domain, given its catalog and its shard list.
    void reuse(const Catalog& catalog, std::span<const ShardReference> earlierShards);

    // Throws `std::invalid_argument` for a path given twice.
    [[nodiscard]] Result build(const io::Package& source, std::vector<Input> inputs);

  private:
    static constexpr std::uint64_t kWholeFileDivisor = 16;

    struct Piece {
        std::uint64_t offset = 0;
        std::uint64_t size = 0;
        Digest contentId;
    };

    struct File {
        Input input;
        std::uint64_t size = 0;
        std::vector<Piece> pieces;
        std::vector<Digest> parts;
    };

    [[nodiscard]] static File identify(const io::Package& source, Input input);
    void keepEarlierShards(const std::vector<File>& files);
    void storeFile(const io::Package& source, File& file);
    [[nodiscard]] Digest storePiece(io::PackageReader& reader, const File& file, const Piece& piece, std::vector<std::uint8_t>& plain);
    void closeShard();

    std::filesystem::path folder;
    std::shared_ptr<const ContentKey> key;
    std::uint64_t target;
    std::shared_ptr<const RecordCache> cache;

    // The chunks of the earlier release by their content IDs, with the position of their shard in the earlier list.
    std::vector<ShardReference> earlierShards;
    std::unordered_map<Digest, Catalog::Chunk, Digest::Hash> earlierChunks;

    // The shards of the result, the kept earlier ones first, and the chunks the files may reference with their position in that list.
    std::vector<ShardReference> shards;
    std::size_t keptShards = 0;
    std::unordered_map<Digest, Catalog::Chunk, Digest::Hash> chunksByContent;
    std::optional<ShardWriter> writer;
    bool fileInShard = false;
    Statistics statistics;
};

} // namespace haylen::content
