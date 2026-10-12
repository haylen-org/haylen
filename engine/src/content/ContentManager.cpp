#include "haylen/content/ContentManager.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_set>
#include <utility>

#include "content/Downloader.hpp"
#include "content/HttpTransport.hpp"
#include "content/ReleasePackage.hpp"
#include "content/UpdateOffer.hpp"
#include "content/Updater.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/io/Path.hpp"

namespace haylen::content {

const std::array<std::pair<std::string_view, ContentManager::Status>, 5> ContentManager::kStatusNames{{{"idle", Status::Idle}, {"checking", Status::Checking}, {"downloading", Status::Downloading}, {"ready", Status::Ready}, {"failed", Status::Failed}}};

std::string_view ContentManager::getStatusName(Status value) noexcept {
    return std::ranges::find(kStatusNames, value, &std::pair<std::string_view, Status>::second)->first;
}

ContentManager::ContentManager(core::Engine& owner, std::shared_ptr<io::Package> appPackage, const core::AppConfig& config) : engine(owner), release(std::dynamic_pointer_cast<ReleasePackage>(std::move(appPackage))), settings(config.content) {
    if (!release || !release->getStore()) {
        return;
    }
    transport = std::make_unique<HttpTransport>(engine.getScriptRuntime());
    downloader = std::make_unique<Downloader>(engine.getJobs(), *transport, release->getStore());
    updater = std::make_unique<Updater>(release, engine.getJobs(), *transport, *downloader, settings);
}

ContentManager::~ContentManager() = default;

bool ContentManager::isProtected() const noexcept {
    return release != nullptr;
}

std::uint64_t ContentManager::getGeneration() const {
    return release ? release->getContent()->manifest->getEnvelope().generation : 0;
}

std::string ContentManager::getChannel() const {
    return settings.updates ? settings.updates->channel : std::string();
}

std::optional<ContentManager::Update> ContentManager::getUpdate() const {
    if (!offer) {
        return std::nullopt;
    }
    return Update{.generation = offer->generation, .downloadBytes = offer->downloadBytes};
}

std::string ContentManager::toPackagePath(std::string_view path) const {
    return io::Path::asset(path);
}

bool ContentManager::isLocal(std::string_view path) const {
    // An asset that does not exist is no download to wait for, and its load reports that it is missing.
    if (!engine.getPackage().exists(toPackagePath(path))) {
        return true;
    }
    if (!release) {
        return settings.getDelivery(io::Path::normalize(path)) == Delivery::Required || broughtInDevelopment.contains(io::Path::normalize(path));
    }
    return release->isLocal(toPackagePath(path));
}

void ContentManager::addPackFolder(const std::filesystem::path& folder) {
    if (release) {
        release->addPackFolder(folder);
    }
}

bool ContentManager::hasServer() const noexcept {
    return settings.updates.has_value() || !release->getRemoteAddress().empty();
}

std::string ContentManager::locate(const ShardReference& pack) const {
    const std::vector<ShardReference>& shipped = release->getBundledContent()->manifest->getEnvelope().shards;
    if (!release->getRemoteAddress().empty() && std::ranges::find(shipped, pack) != shipped.end()) {
        return release->getRemoteAddress() + pack.getFileName();
    }
    return updater->locate(pack);
}

void ContentManager::fail(const Error& failure) {
    status = Status::Failed;
    error = failure;
}

void ContentManager::start() {
    if (!release) {
        return;
    }
    // The app hears of a fall back once it runs, so the listeners that its main module adds receive it.
    if (const std::optional<ReleasePackage::Recovery> recovery = release->takeRecovery()) {
        const core::Json report = {{"manifest", recovery->failed.toHex()}, {"reason", recovery->reason}};
        recoveryReport = engine.getEvents().on(core::LifecycleEvent::kAppStarted, [this, report](core::EventBus::Event&) { engine.getEvents().emit(core::LifecycleEvent::kContentRecovered, report); }, {.once = true});
    }
    if (!downloader) {
        return;
    }
    startHold = engine.holdStart();
    holdingStart = true;
    bringRequired();
}

void ContentManager::bringRequired() {
    status = Status::Downloading;
    error.reset();
    progress = {};
    const std::shared_ptr<const ReleasePackage::Content> content = release->getContent();

    // Finding what the device lacks opens the shard files it looks into, so it runs away from the frame thread.
    // clang-format off
    engine.getJobs().run([content, rules = settings] { return Downloader::findMissing(*content->shards, Updater::getChunks(*content->catalog, Delivery::Required, rules)); }, [this, owner = std::weak_ptr(alive), content](core::JobSystem::Result<std::vector<Catalog::Chunk>> missing) {
        if (owner.expired()) {
            return;
        }
        if (!missing.isOk()) {
            fail(Error(Error::Code::CacheCorrupt, missing.error));
            return;
        }
        if (missing.value->empty()) {
            finishStart();
            return;
        }
        if (!hasServer()) {
            fail(Error(Error::Code::StoreAssetUnavailable, "The content that the app needs to start is not on the device, and the app names no server to download it from. Install the app again."));
            return;
        }
        progress = {.totalBytes = Downloader::measure(*missing.value, *release->getStore())};
        downloader->fetch(content->shards, *missing.value, [this](const ShardReference& pack) { return locate(pack); }, {
            .onProgress = [this](const Downloader::Progress& received) { progress = {.receivedBytes = received.receivedBytes, .totalBytes = received.totalBytes}; },
            .onComplete = [this, owner](std::optional<Error> failure) {
                if (owner.expired()) {
                    return;
                }
                if (!failure) {
                    finishStart();
                    return;
                }

                // Downloaded content that cannot come back in full never keeps the app from starting while the content it ships with can play.
                if (release->getContent() != release->getBundledContent()) {
                    core::Log::warning("The content of the app could not be brought back, so it plays the content it ships with: {}", failure->what());
                    release->mount(release->getBundledContent());
                    bringRequired();
                    return;
                }
                fail(*failure);
            },
        });
    });
    // clang-format on
}

void ContentManager::finishStart() {
    status = Status::Idle;
    holdingStart = false;
    startHold.reset();

    // The required content stays readable for the whole run, which matters where the store reads records in from elsewhere, as on the web.
    std::vector<Digest> required;
    for (const Catalog::Chunk& chunk : Updater::getChunks(*release->getContent()->catalog, Delivery::Required, settings)) {
        required.push_back(chunk.storedId);
    }
    release->getStore()->pin(required);

    // The store forgets what no retained content uses before anything new arrives, and the background work begins after that.
    // clang-format off
    updater->collect([this, owner = std::weak_ptr(alive)](Updater::Collection, std::optional<Error> failure) {
        if (owner.expired()) {
            return;
        }
        if (failure) {
            core::Log::warning("The content store could not drop what the app no longer uses: {}", failure->what());
        }
        if (!hasServer()) {
            return;
        }
        prefetch();
        if (settings.updates && settings.updates->enabled) {
            startCheck();
        }
    });
    // clang-format on
}

void ContentManager::prefetch() {
    const std::shared_ptr<const ReleasePackage::Content> content = release->getContent();
    // clang-format off
    engine.getJobs().run([content, rules = settings] { return Downloader::findMissing(*content->shards, Updater::getChunks(*content->catalog, Delivery::Prefetch, rules)); }, [this, owner = std::weak_ptr(alive), content](core::JobSystem::Result<std::vector<Catalog::Chunk>> missing) {
        if (owner.expired() || !missing.isOk() || missing.value->empty()) {
            return;
        }
        downloader->fetch(content->shards, *missing.value, [this](const ShardReference& pack) { return locate(pack); }, {
            .onComplete = [](std::optional<Error> failure) {
                if (failure) {
                    core::Log::warning("The prefetched content stopped downloading, and it downloads when the app asks for it: {}", failure->what());
                }
            },
        });
    });
    // clang-format on
}

void ContentManager::startCheck() {
    // clang-format off
    check([this](std::optional<Update> found, std::optional<Error> failure) {
        if (failure || !found) {
            return;
        }
        download({}, [](std::optional<Error>) {});
    });
    // clang-format on
}

void ContentManager::acquire(const std::vector<std::string>& paths, ProgressCallback onProgress, Completion done) {
    std::vector<std::string> wanted;
    for (const std::string& path : paths) {
        if (!engine.getPackage().exists(toPackagePath(path))) {
            throw std::invalid_argument("The asset \"" + path + "\" does not exist.");
        }
        if (!isLocal(path)) {
            wanted.push_back(io::Path::normalize(path));
        }
    }

    // Development has no downloads, and what it brings is there in the next frame, as fast content arrives in a release.
    if (!release || wanted.empty()) {
        // clang-format off
        engine.getJobs().postToFrame([this, owner = std::weak_ptr(alive), wanted, onProgress = std::move(onProgress), done = std::move(done)] {
            if (owner.expired()) {
                return;
            }
            broughtInDevelopment.insert(wanted.begin(), wanted.end());
            if (onProgress) {
                onProgress({.receivedBytes = 1, .totalBytes = 1});
            }
            done(std::nullopt);
        });
        // clang-format on
        return;
    }
    if (!downloader || !hasServer()) {
        // clang-format off
        engine.getJobs().postToFrame([path = wanted.front(), done = std::move(done)] {
            done(Error(Error::Code::StoreAssetUnavailable, "The asset \"" + path + "\" is not on the device, and the app names no server to download it from."));
        });
        // clang-format on
        return;
    }

    std::vector<Catalog::Chunk> chunks;
    const std::shared_ptr<const ReleasePackage::Content> content = release->getContent();
    for (const std::string& path : wanted) {
        const Catalog::File file = content->catalog->getFile(*content->catalog->findFile(toPackagePath(path)));
        for (std::uint64_t part = file.firstPart; part < file.firstPart + file.partCount; ++part) {
            chunks.push_back(content->catalog->getChunk(content->catalog->getPart(part).chunk));
        }
    }
    // clang-format off
    engine.getJobs().run([content, chunks = std::move(chunks)] { return Downloader::findMissing(*content->shards, chunks); }, [this, owner = std::weak_ptr(alive), content, onProgress = std::move(onProgress), done = std::move(done)](core::JobSystem::Result<std::vector<Catalog::Chunk>> missing) mutable {
        if (owner.expired()) {
            return;
        }
        if (!missing.isOk()) {
            done(Error(Error::Code::CacheCorrupt, missing.error));
            return;
        }
        downloader->fetch(content->shards, *missing.value, [this](const ShardReference& pack) { return locate(pack); }, {
            .onProgress = [onProgress = std::move(onProgress)](const Downloader::Progress& received) {
                if (onProgress) {
                    onProgress({.receivedBytes = received.receivedBytes, .totalBytes = received.totalBytes});
                }
            },
            .onComplete = std::move(done),
        });
    });
    // clang-format on
}

void ContentManager::check(std::function<void(std::optional<Update>, std::optional<Error>)> done) {
    if (!release) {
        throw std::logic_error("The app plays its folder in development, which has no updates. Build it in the Release configuration to check for updates.");
    }
    if (!updater || !settings.updates) {
        throw std::logic_error("The app names no content server, so it has no updates. Name one in \"content.updates\" of \"app.json\".");
    }
    status = Status::Checking;
    error.reset();
    // clang-format off
    updater->check([this, owner = std::weak_ptr(alive), done = std::move(done)](std::optional<UpdateOffer> found, std::optional<Error> failure) {
        if (owner.expired()) {
            return;
        }
        if (failure) {
            fail(*failure);
            engine.getEvents().emit(core::LifecycleEvent::kContentUpdateFailed, {{"code", Error::getCodeName(failure->getCode())}, {"message", failure->what()}});
            done(std::nullopt, std::move(failure));
            return;
        }
        status = installed ? Status::Ready : Status::Idle;
        if (!found) {
            done(std::nullopt, std::nullopt);
            return;
        }
        offer = std::make_unique<UpdateOffer>(std::move(*found));
        installed = false;
        engine.getEvents().emit(core::LifecycleEvent::kContentUpdateAvailable, {{"generation", offer->generation}, {"downloadBytes", offer->downloadBytes}});
        done(getUpdate(), std::nullopt);
    });
    // clang-format on
}

void ContentManager::download(ProgressCallback onProgress, Completion done) {
    if (offer) {
        installOffer(std::move(onProgress), std::move(done));
        return;
    }
    // clang-format off
    check([this, onProgress = std::move(onProgress), done = std::move(done)](std::optional<Update> found, std::optional<Error> failure) mutable {
        if (failure || !found) {
            done(std::move(failure));
            return;
        }
        installOffer(std::move(onProgress), std::move(done));
    });
    // clang-format on
}

void ContentManager::installOffer(ProgressCallback onProgress, Completion done) {
    status = Status::Downloading;
    error.reset();
    progress = {.totalBytes = offer->downloadBytes};
    // clang-format off
    updater->install(*offer, [this, onProgress](const Downloader::Progress& received) {
        progress = {.receivedBytes = received.receivedBytes, .totalBytes = received.totalBytes};
        if (onProgress) {
            onProgress(progress);
        }
    }, [this, owner = std::weak_ptr(alive), done = std::move(done)](std::optional<Error> failure) {
        if (owner.expired()) {
            return;
        }
        if (failure) {
            fail(*failure);
            engine.getEvents().emit(core::LifecycleEvent::kContentUpdateFailed, {{"code", Error::getCodeName(failure->getCode())}, {"message", failure->what()}});
            done(std::move(failure));
            return;
        }
        status = Status::Ready;
        installed = true;
        engine.getEvents().emit(core::LifecycleEvent::kContentUpdateReady, {{"generation", offer->generation}});
        done(std::nullopt);
    });
    // clang-format on
}

void ContentManager::apply() {
    if (!installed) {
        throw std::logic_error("No content update is ready to apply. Download one first.");
    }
    release->mount(offer->content);
    engine.requestRestart();
}

void ContentManager::cancel() {
    if (updater) {
        updater->cancel();
    }
}

void ContentManager::retry() {
    if (holdingStart && status == Status::Failed) {
        bringRequired();
    }
}

void ContentManager::update() {
    if (downloader) {
        downloader->update();
    }
}

ContentManager::Statistics ContentManager::getStatistics() const {
    Statistics statistics{.generation = getGeneration()};
    if (release) {
        statistics.catalogFiles = release->getContent()->catalog->getFileCount();
    }
    if (downloader) {
        const LocalStore::Statistics stored = release->getStore()->getStatistics();
        statistics.storedRecords = stored.records;
        statistics.storedBytes = stored.segmentBytes;
        statistics.downloadedBytes = downloader->getStatistics().downloadedBytes;
        statistics.requests = downloader->getStatistics().requests;
        statistics.resumedRecords = downloader->getStatistics().resumedRecords;
        statistics.pendingDownloads = downloader->isIdle() ? 0 : 1;
    }
    return statistics;
}

} // namespace haylen::content
