#include "haylen/content/ContentManager.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <unordered_set>
#include <utility>

#include "content/Downloader.hpp"
#include "content/HttpTransport.hpp"
#include "content/ReleasePackage.hpp"
#include "content/UpdateOffer.hpp"
#include "content/Updater.hpp"
#include "haylen/assets/Manager.hpp"
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

bool ContentManager::isGroupLocal(std::string_view group) const {
    return std::ranges::all_of(engine.getAssets().getGroupPaths(group), [this](const std::string& path) { return isLocal(path); });
}

void ContentManager::addPackFolder(const std::filesystem::path& folder) {
    if (release) {
        release->addPackFolder(folder);
    }
}

void ContentManager::PackSourceLink::disconnect() {
    source.reset();
    const std::map<std::uint64_t, std::function<void()>> abandoned = std::exchange(waiting, {});
    for (const auto& [request, abandon] : abandoned) {
        abandon();
    }
}

core::Connection ContentManager::addPackSource(std::shared_ptr<PackSource> source) {
    std::erase_if(packSources, [](const std::shared_ptr<PackSourceLink>& link) { return !link->isConnected(); });
    const std::shared_ptr<PackSourceLink>& link = packSources.emplace_back(std::make_shared<PackSourceLink>(std::move(source)));
    return core::Connection(link);
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
    listen();
    startHold = engine.holdStart();
    holdingStart = true;
    bringRequired();
}

void ContentManager::listen() {
    core::EventBus& events = engine.getEvents();
    listeners.emplace_back(events.on(core::LifecycleEvent::kNetworkOnline, [this](core::EventBus::Event&) { resumeOnline(); }));
    listeners.emplace_back(events.on(core::LifecycleEvent::kAppBackground, [this](core::EventBus::Event&) { pauseBackground(); }));
    listeners.emplace_back(events.on(core::LifecycleEvent::kAppActive, [this](core::EventBus::Event&) { resumeBackground(); }));
}

void ContentManager::resumeOnline() {
    const bool retryable = status == Status::Failed && error && error->isRetryable();
    if (holdingStart) {
        if (retryable) {
            bringRequired();
        }
        return;
    }
    if (prefetchWanted && !prefetching && !inBackground) {
        prefetch();
    }
    if (checkAgain && retryable) {
        checkAgain = false;
        startCheck();
    }
}

void ContentManager::pauseBackground() {
    inBackground = true;
    if (prefetchRequest != 0) {
        downloader->cancel(std::exchange(prefetchRequest, 0));
    }
}

void ContentManager::resumeBackground() {
    inBackground = false;
    if (prefetchWanted && !prefetching) {
        prefetch();
    }
}

std::optional<Error> ContentManager::checkRoom(std::uint64_t downloadBytes, std::uint64_t records, std::uint64_t available) const {
    const std::uint64_t required = Updater::getRequiredSpace(downloadBytes, records);
    if (required <= available) {
        return std::nullopt;
    }
    return Error(Error::Code::InsufficientStorage, std::format("The content needs {} bytes of room on the device and {} are free. Free some room and try again.", required, available), {.requiredBytes = required, .availableBytes = available});
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
    prefetchWanted = true;
    prefetching = true;
    const std::shared_ptr<const ReleasePackage::Content> content = release->getContent();
    // clang-format off
    engine.getJobs().run([content, rules = settings, store = release->getStore()] {
        return std::pair(Downloader::findMissing(*content->shards, Updater::getChunks(*content->catalog, Delivery::Prefetch, rules)), store->getAvailableSpace());
    }, [this, owner = std::weak_ptr(alive), content](core::JobSystem::Result<std::pair<std::vector<Catalog::Chunk>, std::uint64_t>> found) {
        if (owner.expired()) {
            return;
        }
        prefetching = false;
        if (!found.isOk() || found.value->first.empty()) {
            prefetchWanted = false;
            return;
        }
        // The app went to the background meanwhile, and the prefetch begins once it is active again.
        if (inBackground) {
            return;
        }
        const std::vector<Catalog::Chunk>& missing = found.value->first;
        if (const std::optional<Error> lacking = checkRoom(Downloader::measure(missing, *release->getStore()), missing.size(), found.value->second)) {
            core::Log::warning("The prefetched content does not download, and it downloads when the app asks for it: {}", lacking->what());
            prefetchWanted = false;
            return;
        }
        prefetching = true;
        prefetchRequest = downloader->fetch(content->shards, missing, [this](const ShardReference& pack) { return locate(pack); }, {
            .onComplete = [this, owner](std::optional<Error> failure) {
                if (owner.expired()) {
                    return;
                }
                prefetching = false;
                prefetchRequest = 0;
                if (failure && failure->getCode() == Error::Code::NetworkCancelled) {
                    if (!inBackground) {
                        prefetch();
                    }
                    return;
                }
                if (failure) {
                    core::Log::warning("The prefetched content stopped downloading, and it goes on when the network comes back or downloads when the app asks for it: {}", failure->what());
                    return;
                }
                prefetchWanted = false;
            },
        }, Downloader::Priority::Background);
    });
    // clang-format on
}

void ContentManager::startCheck() {
    // A check or an install that the manager starts by itself starts again when the network comes back after it failed for want of it.
    // clang-format off
    check([this](std::optional<Update> found, std::optional<Error> failure) {
        if (failure) {
            checkAgain = true;
            return;
        }
        if (!found) {
            return;
        }
        download({}, [this, owner = std::weak_ptr(alive)](std::optional<Error> installFailure) {
            if (!owner.expired() && installFailure) {
                checkAgain = true;
            }
        });
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

    // The assets of packs come from the sources of packs first, and what they leave out from the server.
    std::vector<std::string> packs;
    for (const std::string& path : wanted) {
        const std::string pack(settings.getPack(path));
        if (!pack.empty() && std::ranges::find(packs, pack) == packs.end()) {
            packs.push_back(pack);
        }
    }
    std::erase_if(packSources, [](const std::shared_ptr<PackSourceLink>& link) { return !link->isConnected(); });
    if (packs.empty() || packSources.empty()) {
        bring(std::move(wanted), std::move(onProgress), std::move(done));
        return;
    }
    // clang-format off
    deliverPacks(std::move(packs), 0, std::make_shared<const PackSources>(packSources), onProgress, [this, wanted = std::move(wanted), onProgress, done = std::move(done)](std::optional<Error> failure) mutable {
        if (failure) {
            done(std::move(failure));
            return;
        }
        std::erase_if(wanted, [this](const std::string& path) { return isLocal(path); });
        if (wanted.empty()) {
            done(std::nullopt);
            return;
        }
        bring(std::move(wanted), std::move(onProgress), std::move(done));
    });
    // clang-format on
}

void ContentManager::deliverPacks(std::vector<std::string> packs, std::size_t index, std::shared_ptr<const PackSources> sources, ProgressCallback onProgress, Completion done) {
    if (index == packs.size()) {
        done(std::nullopt);
        return;
    }
    std::string pack = packs[index];
    // clang-format off
    askSource(std::move(pack), sources, 0, onProgress, [this, packs = std::move(packs), index, sources, onProgress, done = std::move(done)](std::optional<Error> failure) mutable {
        if (failure) {
            done(std::move(failure));
            return;
        }
        deliverPacks(std::move(packs), index + 1, std::move(sources), std::move(onProgress), std::move(done));
    });
    // clang-format on
}

void ContentManager::askSource(std::string pack, std::shared_ptr<const PackSources> sources, std::size_t index, ProgressCallback onProgress, Completion done) {
    while (index < sources->size() && (!(*sources)[index]->isConnected() || (*sources)[index]->isBlocked())) {
        ++index;
    }
    if (index == sources->size()) {
        done(std::nullopt);
        return;
    }
    const std::shared_ptr<PackSourceLink>& link = (*sources)[index];
    const std::uint64_t request = ++packRequests;

    // The source answers once, and a source that its connection removes first answers that it does not deliver the pack, in the next frame.
    // clang-format off
    auto answer = std::make_shared<std::function<void(std::optional<std::filesystem::path>, std::optional<Error>)>>();
    *answer = [this, owner = std::weak_ptr(alive), held = std::weak_ptr(link), request, pack, sources, index, onProgress, done = std::move(done)](std::optional<std::filesystem::path> folder, std::optional<Error> failure) mutable {
        const std::shared_ptr<PackSourceLink> source = held.lock();
        if (owner.expired() || !done) {
            return;
        }
        Completion finish = std::exchange(done, nullptr);
        if (source) {
            source->waiting.erase(request);
        }
        if (failure) {
            finish(std::move(failure));
            return;
        }
        if (!folder) {
            askSource(std::move(pack), std::move(sources), index + 1, std::move(onProgress), std::move(finish));
            return;
        }
        try {
            addPackFolder(*folder);
        } catch (const std::runtime_error&) {
            finish(Error(Error::Code::StoreAssetUnavailable, "The source of packs delivered the pack \"" + pack + "\" into \"" + folder->generic_string() + "\", which is no folder."));
            return;
        }
        finish(std::nullopt);
    };
    link->waiting.emplace(request, [&jobs = engine.getJobs(), owner = std::weak_ptr(alive), answer] {
        jobs.postToFrame([owner, answer] {
            if (!owner.expired()) {
                (*answer)(std::nullopt, std::nullopt);
            }
        });
    });
    link->source->acquire(pack, [owner = std::weak_ptr(alive), onProgress](const PackSource::Progress& received) {
        if (!owner.expired() && onProgress) {
            onProgress({.receivedBytes = received.receivedBytes, .totalBytes = received.totalBytes});
        }
    }, [answer](std::optional<std::filesystem::path> folder, std::optional<Error> failure) {
        (*answer)(std::move(folder), std::move(failure));
    });
    // clang-format on
}

void ContentManager::bring(std::vector<std::string> wanted, ProgressCallback onProgress, Completion done) {
    if (!downloader || !hasServer()) {
        const std::string& path = wanted.front();
        const std::string pack(settings.getPack(path));
        std::string message = pack.empty() ? "The asset \"" + path + "\" is not on the device, and the app names no server to download it from." : "The asset \"" + path + "\" of the pack \"" + pack + "\" is not on the device, no source of packs delivered the pack, and the app names no server to download it from.";
        // clang-format off
        engine.getJobs().postToFrame([message = std::move(message), done = std::move(done)] {
            done(Error(Error::Code::StoreAssetUnavailable, message));
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
    engine.getJobs().run([content, chunks = std::move(chunks), store = release->getStore()] {
        return std::pair(Downloader::findMissing(*content->shards, chunks), store->getAvailableSpace());
    }, [this, owner = std::weak_ptr(alive), content, onProgress = std::move(onProgress), done = std::move(done)](core::JobSystem::Result<std::pair<std::vector<Catalog::Chunk>, std::uint64_t>> found) mutable {
        if (owner.expired()) {
            return;
        }
        if (!found.isOk()) {
            done(Error(Error::Code::CacheCorrupt, found.error));
            return;
        }
        const std::vector<Catalog::Chunk>& missing = found.value->first;
        if (std::optional<Error> lacking = missing.empty() ? std::nullopt : checkRoom(Downloader::measure(missing, *release->getStore()), missing.size(), found.value->second)) {
            done(std::move(lacking));
            return;
        }
        downloader->fetch(content->shards, missing, [this](const ShardReference& pack) { return locate(pack); }, {
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
