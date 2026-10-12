#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/content/Error.hpp"
#include "haylen/core/AppConfig.hpp"

namespace haylen::core {
class Engine;
}

namespace haylen::io {
class Package;
}

namespace haylen::content {

class Downloader;
class HttpTransport;
class ReleasePackage;
class ShardReference;
class Updater;
struct UpdateOffer;

// The content of the running app: which content plays, which of its files are on the device, and its updates. In a protected release it brings the required files that the device lacks before the app starts, downloads the prefetched files in the background once the app runs, brings the files that load on demand when the app asks for them, and checks the update channel of the app and installs newer content that plays from the next start. In development it plays the folder of the app, where the files that `app.json` delivers later are on the device only once something brought them, as they would be in a release, so an app learns early that it loads them the way a release needs. Its callbacks run on the frame thread, and it never shows cryptographic details.
class ContentManager final {
  public:
    // Idle when nothing runs, Checking while it asks the channel, Downloading while it brings files or an update, Ready once an update is installed and plays from the next start, and Failed after an error, which `getError` holds.
    enum class Status : std::uint8_t {
        Idle,
        Checking,
        Downloading,
        Ready,
        Failed,
    };

    struct Progress {
        std::uint64_t receivedBytes = 0;
        std::uint64_t totalBytes = 0;
    };

    // Newer content that the channel offers: its generation and the bytes that installing it downloads.
    struct Update {
        std::uint64_t generation = 0;
        std::uint64_t downloadBytes = 0;
    };

    struct Statistics {
        std::uint64_t generation = 0;
        std::uint64_t catalogFiles = 0;
        std::uint64_t storedRecords = 0;
        std::uint64_t storedBytes = 0;
        std::uint64_t downloadedBytes = 0;
        std::uint64_t requests = 0;
        std::uint64_t resumedRecords = 0;
        std::uint64_t pendingDownloads = 0;
    };

    using ProgressCallback = std::function<void(const Progress&)>;
    using Completion = std::function<void(std::optional<Error>)>;

    ContentManager(core::Engine& owner, std::shared_ptr<io::Package> appPackage, const core::AppConfig& config);
    ~ContentManager();

    ContentManager(const ContentManager&) = delete;
    ContentManager& operator=(const ContentManager&) = delete;

    // Holds the start of the app until its required files are on the device, then collects what the store no longer needs, downloads the prefetched files and, with updates on, checks the channel.
    void start();

    // Reports progress and drives the downloads once per frame.
    void update();

    // Whether the app plays a protected release, as it does in a release build.
    [[nodiscard]] bool isProtected() const noexcept;

    // The generation of the content that plays: 1 for the content that ships with a release, the generation of its channel for an update, and 0 in development.
    [[nodiscard]] std::uint64_t getGeneration() const;
    [[nodiscard]] std::string getChannel() const;

    [[nodiscard]] Status getStatus() const noexcept {
        return status;
    }
    [[nodiscard]] const std::optional<Error>& getError() const noexcept {
        return error;
    }
    [[nodiscard]] const Progress& getProgress() const noexcept {
        return progress;
    }

    // The update that the last check found, until it is installed.
    [[nodiscard]] std::optional<Update> getUpdate() const;

    // Whether the app waits for its required files before it starts, while the engine shows their download.
    [[nodiscard]] bool isHoldingStart() const noexcept {
        return holdingStart;
    }

    // Whether every byte of an asset, by its path relative to `content`, is on the device, so loading it never waits for a download.
    [[nodiscard]] bool isLocal(std::string_view path) const;

    // Brings the assets that the device lacks, by their paths relative to `content`, and calls back once all of them are on the device. Throws `std::invalid_argument` for an asset that does not exist.
    void acquire(const std::vector<std::string>& paths, ProgressCallback onProgress, Completion done);

    // Asks the channel for newer content. Throws `std::logic_error` in an app without a content server.
    void check(std::function<void(std::optional<Update>, std::optional<Error>)> done);

    // Installs the update that the last check found, checking first when none was found, as the content of the next start.
    void download(ProgressCallback onProgress, Completion done);

    // Starts the app again on the installed update, which a check and a download made ready. Throws `std::logic_error` when no update is ready.
    void apply();

    // Stops the check or the download that runs, which fail with `NetworkCancelled`, and tries the required files again after a failure while the app waits for them.
    void cancel();
    void retry();

    [[nodiscard]] Statistics getStatistics() const;

    // The name of a status as Lua reads it, such as `downloading`.
    [[nodiscard]] static std::string_view getStatusName(Status value) noexcept;

  private:
    static const std::array<std::pair<std::string_view, Status>, 5> kStatusNames;

    void startCheck();
    void installOffer(ProgressCallback onProgress, Completion done);
    void bringRequired();
    void finishStart();
    void prefetch();
    void fail(const Error& failure);

    // Whether the content has a server to download from: the server of `app.json`, or the place that the release of a web page stays.
    [[nodiscard]] bool hasServer() const noexcept;

    // The address of a shard: next to the release on a web page for the shards that ship with the app, or in the publication tree of the server.
    [[nodiscard]] std::string locate(const ShardReference& pack) const;
    [[nodiscard]] std::string toPackagePath(std::string_view path) const;

    core::Engine& engine;
    std::shared_ptr<ReleasePackage> release;
    core::AppConfig::Content settings;
    std::unique_ptr<HttpTransport> transport;
    std::unique_ptr<Downloader> downloader;
    std::unique_ptr<Updater> updater;
    std::unique_ptr<UpdateOffer> offer;
    std::shared_ptr<void> startHold;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
    std::set<std::string> broughtInDevelopment;
    Status status = Status::Idle;
    std::optional<Error> error;
    Progress progress;
    bool holdingStart = false;
    bool installed = false;
};

} // namespace haylen::content
