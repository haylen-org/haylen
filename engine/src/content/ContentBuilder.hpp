#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "content/Delivery.hpp"
#include "content/ShardWriter.hpp"
#include "content/crypto/ContentKey.hpp"
#include "content/crypto/Digest.hpp"
#include "content/format/Catalog.hpp"
#include "content/format/ShardReference.hpp"
#include "haylen/io/Package.hpp"

namespace haylen::content {

// Builds the shards and the plain catalog of one domain from the files of a source package. Files are taken in path order and read in windows, so no file is ever loaded whole, and each is split into chunks that are identified, compressed and sealed. A chunk whose content an earlier release already stored is referenced in its old shard instead of being stored again, so an update writes only new chunks into new shards and never touches the old ones. New shards close near the target size, and a file small against that target stays in one shard.
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
        std::uint64_t reusedChunks = 0;
        std::uint64_t reusedBytes = 0;
    };

    struct Result {
        std::vector<std::uint8_t> catalog;
        std::vector<ShardReference> shards;
        Statistics statistics;
    };

    ContentBuilder(std::filesystem::path outputFolder, std::shared_ptr<const ContentKey> contentKey, std::uint64_t shardTarget = kDefaultShardTarget);

    // Lets the build reference the chunks of an earlier release of the same domain, given its catalog and its shard list.
    void reuse(const Catalog& catalog, std::span<const ShardReference> earlierShards);

    // Throws `std::invalid_argument` for a path given twice.
    [[nodiscard]] Result build(const io::Package& source, std::vector<Input> inputs);

  private:
    static constexpr std::uint64_t kWholeFileDivisor = 16;

    struct File {
        std::string path;
        Delivery delivery = Delivery::Required;
        std::vector<Digest> parts;
    };

    void addFile(const io::Package& source, const Input& input);
    [[nodiscard]] Digest addChunk(std::span<const std::uint8_t> plain, std::uint64_t fileSize);
    void closeShard();

    std::filesystem::path folder;
    std::shared_ptr<const ContentKey> key;
    std::uint64_t target;

    // Shards are numbered first by the earlier release and then in the order this build creates them, and the result keeps only the ones its files use.
    std::vector<ShardReference> shards;
    std::unordered_map<Digest, Catalog::Chunk, Digest::Hash> chunksByContent;
    std::vector<File> files;
    std::optional<ShardWriter> writer;
    bool fileInShard = false;
    Statistics statistics;
};

} // namespace haylen::content
