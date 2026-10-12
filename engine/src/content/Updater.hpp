#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "content/Downloader.hpp"
#include "content/HttpTransport.hpp"
#include "content/ReleasePackage.hpp"
#include "content/UpdateOffer.hpp"
#include "content/format/Catalog.hpp"
#include "haylen/content/Delivery.hpp"
#include "haylen/content/Error.hpp"

namespace haylen::core {
class JobSystem;
}

namespace haylen::content {

// Updates the content of an app from the channel of a publication tree as one transaction. A check reads the signed pointer of the channel, rejects a generation older than the one the app accepted last, and downloads, verifies and decrypts the manifest it names. An install checks that the device has room for it, downloads the records of its required files that the device lacks, and only once all of them are verified and durable writes the new state of the store, which names the new content as active and the content before it as previous. A crash, a lost network or a full disk at any point leaves the content the app plays as it was, and the records already downloaded serve the next attempt, which resumes where this one stopped. The new content plays from the next start of the app. It runs on the thread that polls the event loop.
class Updater final {
  public:
    static constexpr std::uint64_t kMaximumDescriptorSize = 64 * 1024;
    static constexpr std::uint64_t kMaximumManifestSize = std::uint64_t{640} << 20;

    // An install keeps at least this much of the device free beyond what it writes, or a twentieth of its download when that is larger.
    static constexpr std::uint64_t kSafetyMargin = std::uint64_t{64} << 20;

    struct Collection {
        std::uint64_t removedRecords = 0;
        std::uint64_t freedBytes = 0;
    };

    // The base address ends with a slash and names the root of the publication tree.
    Updater(std::shared_ptr<ReleasePackage> releasePackage, core::JobSystem& jobSystem, HttpTransport& httpTransport, Downloader& recordDownloader, std::string baseAddress, std::string channelName);
    ~Updater();

    Updater(const Updater&) = delete;
    Updater& operator=(const Updater&) = delete;

    // Asks the channel for newer content and calls back with what it offers, with nothing when the app plays the newest content, or with the error.
    void check(std::function<void(std::optional<UpdateOffer>, std::optional<Error>)> done);

    // Installs an offer as the content of the next start, and calls back with no error once the new state is durable. Throws nothing: a shortage of room fails with `InsufficientStorage`, which names the bytes needed and available.
    void install(const UpdateOffer& offer, std::function<void(const Downloader::Progress&)> progress, std::function<void(std::optional<Error>)> done);

    // Ends the check or install that runs with `NetworkCancelled`.
    void cancel();

    // Removes from the store what neither the content that ships with the app, the active content nor the previous one uses.
    void collect(std::function<void(Collection, std::optional<Error>)> done);

    // The address of a pack of the publication tree.
    [[nodiscard]] std::string locate(const ShardReference& pack) const;

    // The chunks of the files of a catalog with a delivery, each once.
    [[nodiscard]] static std::vector<Catalog::Chunk> getChunks(const Catalog& catalog, Delivery delivery);

  private:
    void fetch(const std::string& url, std::uint64_t maximumSize, std::function<void(std::vector<std::uint8_t>, std::optional<Error>)> done);
    void readOffer(std::uint64_t generation, const Digest& manifest, std::vector<std::uint8_t> bytes, std::function<void(std::optional<UpdateOffer>, std::optional<Error>)> done);
    void commit(const UpdateOffer& offer, std::function<void(std::optional<Error>)> done) const;
    [[nodiscard]] static std::uint64_t getRequiredSpace(const UpdateOffer& offer) noexcept;

    std::shared_ptr<ReleasePackage> release;
    core::JobSystem& jobs;
    HttpTransport& transport;
    Downloader& downloader;
    std::string baseUrl;
    std::string channel;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
    std::vector<HttpTransport::Transfer> transfers;
    std::optional<std::uint64_t> download;
    std::uint64_t checks = 0;
};

} // namespace haylen::content
