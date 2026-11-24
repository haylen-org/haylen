#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "content/HttpTransport.hpp"
#include "content/LocalStore.hpp"
#include "content/ShardReader.hpp"
#include "content/ShardSet.hpp"
#include "content/crypto/Digest.hpp"
#include "content/format/Catalog.hpp"
#include "content/format/PartialRecord.hpp"
#include "content/format/ShardHeader.hpp"
#include "content/format/ShardIndex.hpp"
#include "content/format/ShardReference.hpp"
#include "haylen/content/Error.hpp"

namespace haylen::core {
class JobSystem;
}

namespace haylen::content {

// Brings the chunks that the device lacks into the local store. It asks each shard for its header and its encrypted index once, which the store keeps, then downloads only the records it needs, in few ranges, with a bounded number of requests at once. A server that answers a range with the whole shard, as a content delivery network does with a shard it has not cached yet, hands over every record of the shard, which the store keeps as they arrive, so no later request asks for the shard again. Every record is verified against the signed catalog and the index of its shard on the task pool before the store keeps it, so nothing unverified reaches the store, and nothing plain is ever written. Requests for the same record share one download, a record whose download stopped resumes from its staged first bytes, and cancelling one request leaves the downloads that others still need running. It runs on the thread that polls the event loop, and its callbacks run there.
class Downloader final {
  public:
    // At most this many requests run at once, so a few large records never wait behind many small ones and a slow network is not flooded.
    static constexpr std::size_t kMaximumTransfers = 4;

    // Background work holds at most this many of those requests, so what the app waits for always finds a free one.
    static constexpr std::size_t kMaximumBackgroundTransfers = 1;

    // The records that wait to be checked and written hold memory, and so do the bytes that a store writes later, as on the web, so no transfer starts while they hold more than this, where the checks or the writes fall behind the network.
    static constexpr std::uint64_t kMaximumHeldBytes = std::uint64_t{16} << 20;

    // A record whose download stops keeps its bytes for a resume once it holds at least this much.
    static constexpr std::size_t kMinimumPartialSize = 64 * 1024;

    // Foreground requests are what the app waits for, such as its required content, an asset it loads or an update, and background requests are content it may need later, which downloads only while nothing in the foreground waits.
    enum class Priority : std::uint8_t {
        Foreground,
        Background,
    };

    // The address of the file of a shard.
    using Locator = std::function<std::string(const ShardReference&)>;

    struct Progress {
        std::uint64_t receivedBytes = 0;
        std::uint64_t totalBytes = 0;
    };

    // What a request brings records for: to read them at once, as a load does, which where the store keeps records elsewhere, as on the web, reads them in again before the request ends, or to keep them on the device, as a prefetch or an update does, which leaves out the records that the store holds already.
    enum class Use : std::uint8_t {
        Read,
        Keep,
    };

    struct Statistics {
        std::uint64_t downloadedBytes = 0;
        std::uint64_t storedRecords = 0;
        std::uint64_t requests = 0;
        std::uint64_t resumedRecords = 0;
    };

    struct Callbacks {
        std::function<void(const Progress&)> onProgress{};

        // Runs exactly once, with no error when every chunk is on the device.
        std::function<void(std::optional<Error>)> onComplete;
    };

    Downloader(core::JobSystem& jobSystem, HttpTransport& httpTransport, std::shared_ptr<LocalStore> localStore);
    ~Downloader();

    Downloader(const Downloader&) = delete;
    Downloader& operator=(const Downloader&) = delete;

    // The chunks that the device holds neither in the file of their shard nor in the store, each once. It opens the shard files it looks into, so it runs away from the frame thread.
    [[nodiscard]] static std::vector<Catalog::Chunk> findMissing(const ShardSet& shards, std::span<const Catalog::Chunk> chunks);

    // The bytes that downloading chunks takes: the sizes of their records, without the ones the store holds and reads in instead, as a web page does with what an earlier visit downloaded.
    [[nodiscard]] static std::uint64_t measure(std::span<const Catalog::Chunk> chunks, const LocalStore& store);

    // Brings chunks that the device lacked when `findMissing` looked, and returns the ID of the request. Chunks that the store received since are skipped. A record that a foreground request also waits for downloads in the foreground.
    std::uint64_t fetch(std::shared_ptr<const ShardSet> shards, std::span<const Catalog::Chunk> chunks, const Locator& locate, Callbacks callbacks, Priority priority = Priority::Foreground, Use use = Use::Keep);

    // Ends a request with `NetworkCancelled`. Downloads that other requests still need go on.
    void cancel(std::uint64_t request);

    // Reports the progress of the requests that received bytes since the last call, once per frame, and starts the transfers that waited for the store to write what it held.
    void update();

    [[nodiscard]] const Statistics& getStatistics() const noexcept {
        return statistics;
    }
    [[nodiscard]] bool isIdle() const noexcept {
        return requests.empty();
    }

    // The records that requests still wait for, and the range requests that run now.
    [[nodiscard]] std::size_t getWantedRecords() const noexcept {
        return wanted.size();
    }
    [[nodiscard]] std::size_t getActiveTransfers() const noexcept {
        return activeTransfers.size();
    }

  private:
    struct Request {
        Callbacks callbacks;
        Priority priority = Priority::Foreground;
        std::set<Digest> waiting;

        // Every record a request to read brings, which it reads in before it ends where the store keeps records elsewhere, as on the web.
        std::vector<Digest> records;
        Progress progress;
        bool changed = false;

        // The first failure of a request, which it reports once the records that arrived before it are on the device.
        std::optional<Error> failure;
    };

    // A record that requests wait for, with what it must be.
    struct Wanted {
        Catalog::Chunk chunk;
        Digest shard;
        std::vector<std::uint64_t> requests;
        bool transferring = false;
        bool verifying = false;

        // Whether the download of the record went on from its staged first bytes, which a damaged device may have changed.
        bool resumed = false;
    };

    struct Shard {
        ShardReference reference;
        std::string url;
        std::shared_ptr<const KeyRing> keys;
        std::shared_ptr<const ShardReader> index;
        bool indexing = false;
        std::vector<Digest> queued;
    };

    // One range request and the records it carries, in the order of the shard.
    struct Transfer {
        Digest shard;
        std::vector<std::pair<Digest, ShardIndex::Entry>> records;
        std::size_t current = 0;
        std::uint64_t position = 0;
        std::vector<std::uint8_t> buffer;
        std::string validator;
        HttpTransport::Transfer handle;
        bool abandoned = false;

        // Whether the server answered the range with the whole shard, which the downloader keeps to its end.
        bool whole = false;
    };

    // A whole shard that a server sent in place of a range, read in the order of its file: the header, the header and the rest of each record, and then the index. Each piece waits whole in `piece`, between `pieceStart` and `pieceEnd` of the file, so one record at a time stays in memory.
    struct WholeShard {
        std::uint64_t position = 0;
        std::uint64_t pieceStart = 0;
        std::uint64_t pieceEnd = ShardHeader::kSize;
        std::vector<std::uint8_t> piece;
        std::vector<std::uint8_t> headerBytes;
        std::optional<ShardHeader> header;
        bool indexed = false;
        std::optional<Error> failure;
    };

    void loadHeld(std::vector<Digest> storedIds);
    void indexShard(Shard& shard);
    void receiveIndex(const Digest& shard, std::vector<std::uint8_t> bytes);
    void failShard(const Digest& shard, const Error& error);
    // Downloads a range of the file of a shard, or keeps the whole shard when the server sends all of it, which then calls `done` with no bytes, since the shard itself brings its index.
    void fetchRange(const Digest& shard, std::uint64_t offset, std::uint64_t size, std::function<void(std::vector<std::uint8_t>, std::optional<Error>)> done);
    void plan(Shard& shard);
    void pump();
    [[nodiscard]] bool isForeground(const Transfer& transfer) const;
    void start(std::shared_ptr<Transfer> transfer);
    void receive(Transfer& transfer, std::span<const std::uint8_t> bytes);
    void finishTransfer(const std::shared_ptr<Transfer>& transfer, std::optional<Error> failure);
    void finishWholeTransfer(const Transfer& transfer, std::optional<Error> failure);
    void verify(const Digest& shard, const Digest& storedId, std::vector<std::uint8_t> record);
    void beginWhole(const Digest& shard);
    void receiveWhole(const Digest& shard, std::span<const std::uint8_t> bytes);
    void receiveWholePiece(const Digest& shard, WholeShard& whole);
    static void startWholePiece(WholeShard& whole);
    void keepRecord(const Digest& shard, const Digest& keyId, std::uint64_t recordOffset, std::vector<std::uint8_t> record);
    void finishWhole(const Digest& shard, std::optional<Error> failure);
    void stored(const Digest& storedId, std::optional<Error> failure);
    void rejected(const Digest& storedId, const Error& error);
    void discardPartial(const Digest& shard, std::uint64_t recordOffset);
    void fail(const Digest& storedId, const Error& error);
    void fail(std::uint64_t request, const Digest& storedId, const Error& error);
    void complete(std::uint64_t request, std::optional<Error> failure);
    void dropUnwanted();

    core::JobSystem& jobs;
    HttpTransport& transport;
    std::shared_ptr<LocalStore> store;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
    std::uint64_t nextRequest = 1;
    std::map<std::uint64_t, Request> requests;
    std::unordered_map<Digest, Wanted, Digest::Hash> wanted;
    std::unordered_map<Digest, Shard, Digest::Hash> shards;
    std::deque<std::shared_ptr<Transfer>> waitingTransfers;
    std::vector<std::shared_ptr<Transfer>> activeTransfers;
    std::map<std::pair<Digest, std::uint64_t>, PartialRecord> partials;
    std::unordered_map<Digest, WholeShard, Digest::Hash> wholes;
    Statistics statistics;
    std::uint64_t verifyingBytes = 0;
};

} // namespace haylen::content
