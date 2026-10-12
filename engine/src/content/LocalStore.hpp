#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "content/StoreFiles.hpp"
#include "content/crypto/Digest.hpp"
#include "content/format/PartialRecord.hpp"
#include "content/format/StoreState.hpp"

namespace haylen::content {

// The local content store of an app: the encrypted records that downloads brought, appended to segments of bounded size, the encrypted shard indexes and the signed manifests they came with, and the state that names the active manifest. Records stay sealed as they arrived, nothing in the store names a path, and no plain byte is ever written. A record becomes part of the store when it is added, and a new state becomes active only once every record it relies on is durable, so a crash leaves either the earlier state or the new one. Records are found by stored ID from any thread, while one thread at a time adds, removes and collects them.
class LocalStore final {
  public:
    // Segments close near this size by default, so the store holds few files without ever rewriting a large one.
    static constexpr std::uint64_t kSegmentTarget = std::uint64_t{256} << 20;

    struct Location {
        std::uint32_t segment = 0;
        std::uint64_t offset = 0;
        std::uint64_t size = 0;
        Digest keyId;
    };

    struct Statistics {
        std::uint64_t records = 0;
        std::uint64_t recordBytes = 0;
        std::uint64_t segments = 0;
        std::uint64_t segmentBytes = 0;
    };

    struct Collection {
        std::uint64_t removedRecords = 0;
        std::uint64_t movedRecords = 0;
        std::uint64_t freedBytes = 0;
    };

    // Opens the store in its files: the newest whole state, the records its journal lists in segments that still hold them, and nothing that a crash or an earlier collection left behind. A store of another app opens empty, and a new build of the app starts again from the content it ships with, keeping the records and the generation it accepted.
    LocalStore(std::unique_ptr<StoreFiles> storeFiles, const Digest& application, std::uint64_t appBuild, std::uint64_t segmentTarget = kSegmentTarget);

    [[nodiscard]] StoreState getState() const;

    // Makes the records added so far durable, then writes the state into the slot that does not hold the current one.
    void commit(StoreState next);

    [[nodiscard]] bool contains(const Digest& storedId) const;

    // Reads a record whole into the buffer with the ID of the key that sealed it, and returns false when the store does not hold it. Throws `CacheCorrupt` when its segment lost it.
    bool readRecord(const Digest& storedId, std::vector<std::uint8_t>& record, Digest& keyId) const;

    // Adds a verified record sealed under the key, unless the store holds it already.
    void addRecord(const Digest& keyId, std::span<const std::uint8_t> record);

    // Forgets a record that failed its checks, so the next download replaces it.
    void removeRecord(const Digest& storedId);
    void flush();

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> readManifest(const Digest& id) const;
    void writeManifest(const Digest& id, std::span<const std::uint8_t> bytes);
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> readShardIndex(const Digest& shard) const;
    void writeShardIndex(const Digest& shard, std::span<const std::uint8_t> bytes);

    // Reads the staged start of a record, or nothing when there is none or it is damaged, which the next download replaces.
    [[nodiscard]] std::optional<PartialRecord> readPartial(const Digest& shard, std::uint64_t recordOffset) const;
    void writePartial(const PartialRecord& partial);

    // Reads every staged start of a record that is whole, when a downloader starts.
    [[nodiscard]] std::vector<PartialRecord> listPartials() const;
    void removePartial(const Digest& shard, std::uint64_t recordOffset);

    // Removes the records outside the live set, and the manifests, shard indexes and staged records that are no longer kept, and moves the live records of segments that are mostly dead into new ones, without ever touching what the live set uses. The new journal becomes active with a new state, so a collection that a crash interrupts leaves the store as it was.
    Collection collect(const std::unordered_set<Digest, Digest::Hash>& liveRecords, const std::set<Digest>& keptManifests, const std::set<Digest>& keptShards);

    [[nodiscard]] Statistics getStatistics() const;
    [[nodiscard]] std::uint64_t getAvailableSpace() const;

  private:
    static constexpr std::string_view kSegmentFolder = "segments";
    static constexpr std::string_view kManifestFolder = "manifests";
    static constexpr std::string_view kIndexFolder = "indexes";
    static constexpr std::string_view kStagingFolder = "staging";
    static constexpr std::array<std::uint8_t, 16> kSegmentHeader = {'H', 'S', 'E', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 1, 0, 0, 0, 0, 0, 0, 0};

    [[nodiscard]] static std::string getSegmentName(std::uint32_t segment);
    [[nodiscard]] static std::string getJournalName(std::uint64_t journal);
    [[nodiscard]] static std::string getStateName(std::uint64_t sequence);
    [[nodiscard]] static std::string getPartialName(const Digest& shard, std::uint64_t recordOffset);

    [[nodiscard]] StoreState readState(const Digest& application, std::uint64_t appBuild) const;
    void readJournal();
    void removeLeftovers();
    void writeState(StoreState next);
    Location appendRecord(const Digest& keyId, std::span<const std::uint8_t> record);

    std::unique_ptr<StoreFiles> files;
    mutable std::shared_mutex mutex;
    std::mutex writeMutex;
    StoreState state;
    std::unordered_map<Digest, Location, Digest::Hash> records;
    std::uint64_t target;
    std::uint32_t appendSegment = 1;
    std::set<std::string> dirty;
};

} // namespace haylen::content
