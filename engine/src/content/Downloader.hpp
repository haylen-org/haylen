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
#include "content/format/ShardIndex.hpp"
#include "content/format/ShardReference.hpp"
#include "haylen/content/Error.hpp"

namespace haylen::core {
class JobSystem;
}

namespace haylen::content {

// Brings the chunks that the device lacks into the local store. It asks each shard for its header and its encrypted index once, which the store keeps, then downloads only the records it needs, in few ranges, with a bounded number of requests at once. Every record is verified against the signed catalog and the index of its shard on the task pool before the store keeps it, so nothing unverified reaches the store, and nothing plain is ever written. Requests for the same record share one download, a record whose download stopped resumes from its staged first bytes, and cancelling one request leaves the downloads that others still need running. It runs on the thread that polls the event loop, and its callbacks run there.
class Downloader final {
  public:
    // At most this many requests run at once, so a few large records never wait behind many small ones and a slow network is not flooded.
    static constexpr std::size_t kMaximumTransfers = 4;

    // Background work holds at most this many of those requests, so what the app waits for always finds a free one.
    static constexpr std::size_t kMaximumBackgroundTransfers = 1;

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

    struct Statistics {
        std::uint64_t downloadedBytes = 0;
        std::uint64_t storedRecords = 0;
        std::uint64_t requests = 0;
        std::uint64_t resumedRecords = 0;
    };

    struct Callbacks {
        std::function<void(const Progress&)> onProgress;

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
    std::uint64_t fetch(std::shared_ptr<const ShardSet> shards, std::span<const Catalog::Chunk> chunks, const Locator& locate, Callbacks callbacks, Priority priority = Priority::Foreground);

    // Ends a request with `NetworkCancelled`. Downloads that other requests still need go on.
    void cancel(std::uint64_t request);

    // Reports the progress of the requests that received bytes since the last call, once per frame.
    void update();

    [[nodiscard]] const Statistics& getStatistics() const noexcept {
        return statistics;
    }
    [[nodiscard]] bool isIdle() const noexcept {
        return requests.empty();
    }

  private:
    struct Request {
        Callbacks callbacks;
        Priority priority = Priority::Foreground;
        std::set<Digest> waiting;
        Progress progress;
        bool changed = false;
    };

    // A record that requests wait for, with what it must be.
    struct Wanted {
        Catalog::Chunk chunk;
        Digest shard;
        std::vector<std::uint64_t> requests;
        bool transferring = false;
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
    };

    void loadHeld(std::vector<Digest> storedIds);
    void indexShard(Shard& shard);
    void receiveIndex(const Digest& shard, std::vector<std::uint8_t> bytes);
    void failShard(const Digest& shard, const Error& error);
    void fetchRange(const std::string& url, std::uint64_t offset, std::uint64_t size, std::function<void(std::vector<std::uint8_t>, std::optional<Error>)> done);
    void plan(Shard& shard);
    void pump();
    [[nodiscard]] bool isForeground(const Transfer& transfer) const;
    void start(std::shared_ptr<Transfer> transfer);
    void receive(Transfer& transfer, std::span<const std::uint8_t> bytes);
    void finishTransfer(const std::shared_ptr<Transfer>& transfer, std::optional<Error> failure);
    void verify(const Digest& shard, const Digest& storedId, std::vector<std::uint8_t> record);
    void stored(const Digest& storedId, std::optional<Error> failure);
    void fail(const Digest& storedId, const Error& error);
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
    Statistics statistics;
};

} // namespace haylen::content
