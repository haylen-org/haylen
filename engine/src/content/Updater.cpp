#include "content/Updater.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <set>
#include <unordered_set>
#include <utility>

#include "content/PublicationLayout.hpp"
#include "content/format/ChannelDescriptor.hpp"
#include "content/format/ChunkRecord.hpp"
#include "content/format/StoreEntry.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/io/Path.hpp"

namespace haylen::content {

Updater::Updater(std::shared_ptr<ReleasePackage> releasePackage, core::JobSystem& jobSystem, HttpTransport& httpTransport, Downloader& recordDownloader, core::AppConfig::Content contentSettings) : release(std::move(releasePackage)), jobs(jobSystem), transport(httpTransport), downloader(recordDownloader), settings(std::move(contentSettings)) {
    if (settings.updates) {
        baseUrl = settings.updates->baseUrl;
        channel = settings.updates->channel;
    }
}

Updater::~Updater() {
    for (const HttpTransport::Transfer& transfer : transfers) {
        transfer.cancel();
    }
}

std::string Updater::locate(const ShardReference& pack) const {
    return baseUrl + PublicationLayout::getPackPath(pack);
}

std::vector<Catalog::Chunk> Updater::getChunks(const Catalog& catalog, Delivery delivery, const core::AppConfig::Content& rules) {
    std::vector<Catalog::Chunk> chunks;
    std::unordered_set<std::uint64_t> listed;
    for (std::uint64_t index = 0; index < catalog.getFileCount(); ++index) {
        const Catalog::File file = catalog.getFile(index);
        if (file.delivery != delivery || isPacked(file.path, rules)) {
            continue;
        }
        for (std::uint64_t part = file.firstPart; part < file.firstPart + file.partCount; ++part) {
            const std::uint64_t chunk = catalog.getPart(part).chunk;
            if (listed.insert(chunk).second) {
                chunks.push_back(catalog.getChunk(chunk));
            }
        }
    }
    return chunks;
}

bool Updater::isPacked(std::string_view path, const core::AppConfig::Content& rules) noexcept {
    const std::string_view folder = io::Path::kContentDirectory;
    return path.size() > folder.size() && path.starts_with(folder) && path[folder.size()] == '/' && !rules.getPack(path.substr(folder.size() + 1)).empty();
}

void Updater::fetch(const std::string& url, std::uint64_t maximumSize, std::function<void(std::vector<std::uint8_t>, std::optional<Error>)> done) {
    auto bytes = std::make_shared<std::vector<std::uint8_t>>();
    // clang-format off
    transfers.push_back(transport.fetch({.url = url, .maximumSize = maximumSize}, {
        .onStart = [](const HttpTransport::Start&) {},
        .onBytes = [bytes](std::span<const std::uint8_t> chunk) { bytes->insert(bytes->end(), chunk.begin(), chunk.end()); },
        .onComplete = [owner = std::weak_ptr(alive), bytes, done = std::move(done)](std::optional<Error> failure) {
            if (!owner.expired()) {
                done(std::move(*bytes), std::move(failure));
            }
        },
    }));
    // clang-format on
    std::erase_if(transfers, [](const HttpTransport::Transfer& transfer) { return !transfer.isActive(); });
}

void Updater::check(std::function<void(std::optional<UpdateOffer>, std::optional<Error>)> done) {
    // The pointer of a channel changes with every release, so its address changes with every check to pass any cache on the way.
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const std::string url = std::format("{}{}?check={}-{}", baseUrl, PublicationLayout::getChannelPath(channel), now, ++checks);
    // clang-format off
    fetch(url, kMaximumDescriptorSize, [this, done = std::move(done)](std::vector<std::uint8_t> bytes, std::optional<Error> failure) mutable {
        if (failure) {
            done(std::nullopt, std::move(failure));
            return;
        }
        std::optional<ChannelDescriptor> descriptor;
        try {
            descriptor = ChannelDescriptor::read(bytes, release->getTrustedKeys());
            if (descriptor->application != release->getCompatibility().application || descriptor->channel != channel) {
                throw Error(Error::Code::CorruptManifest, "The pointer of the channel \"" + channel + "\" names another app or channel.");
            }

            // A server can never take the app back to a generation older than one it accepted, nor offer another manifest under the same one.
            const StoreState state = release->getStore()->getState();
            const bool sameChannel = state.channel == channel;
            if (!descriptor->offersUpdate(sameChannel ? state.acceptedGeneration : 0, sameChannel ? state.acceptedManifest : Digest())) {
                done(std::nullopt, std::nullopt);
                return;
            }
        } catch (const Error& error) {
            done(std::nullopt, error);
            return;
        }
        if (descriptor->manifest == release->getContent()->manifest->getId()) {
            done(std::nullopt, std::nullopt);
            return;
        }
        fetch(baseUrl + PublicationLayout::getManifestPath(descriptor->manifest), kMaximumManifestSize, [this, generation = descriptor->generation, manifest = descriptor->manifest, done = std::move(done)](std::vector<std::uint8_t> manifestBytes, std::optional<Error> manifestFailure) mutable {
            if (manifestFailure) {
                done(std::nullopt, std::move(manifestFailure));
                return;
            }
            readOffer(generation, manifest, std::move(manifestBytes), std::move(done));
        });
    });
    // clang-format on
}

void Updater::readOffer(std::uint64_t generation, const Digest& manifest, std::vector<std::uint8_t> bytes, std::function<void(std::optional<UpdateOffer>, std::optional<Error>)> done) {
    // Verifying, decrypting and measuring the offer runs on the task pool, which also opens the shard files the measure looks into.
    // clang-format off
    jobs.post([&pool = jobs, owner = std::weak_ptr(alive), releasePackage = release, channelName = channel, rules = settings, generation, manifest, bytes = std::move(bytes), done = std::move(done)]() mutable {
        std::optional<UpdateOffer> offer;
        std::optional<Error> failure;
        try {
            auto shared = std::make_shared<const std::vector<std::uint8_t>>(bytes);
            offer = UpdateOffer{.generation = generation, .content = releasePackage->load(std::move(bytes)), .manifest = std::move(shared)};
            const Manifest::Envelope& envelope = offer->content->manifest->getEnvelope();
            if (offer->content->manifest->getId() != manifest || envelope.generation != generation || envelope.channel != channelName) {
                throw Error(Error::Code::CorruptManifest, "The manifest \"" + manifest.toHex() + "\" is not the one its channel names.");
            }
            offer->missing = Downloader::findMissing(*offer->content->shards, getChunks(*offer->content->catalog, Delivery::Required, rules));
            offer->downloadBytes = Downloader::measure(offer->missing, *releasePackage->getStore());
        } catch (const Error& error) {
            offer.reset();
            failure = error;
        }
        pool.postToFrame([owner, offer = std::move(offer), failure = std::move(failure), done = std::move(done)]() mutable {
            if (!owner.expired()) {
                done(std::move(offer), std::move(failure));
            }
        });
    });
    // clang-format on
}

std::uint64_t Updater::getRequiredSpace(const UpdateOffer& offer) noexcept {
    // Beyond its records an install writes their journal entries, the indexes of their shards, the manifest and the staged starts of the records that run at once.
    const std::uint64_t records = offer.missing.size();
    const std::uint64_t overhead = records * (StoreEntry::kSize + ShardIndex::kEntrySize) + offer.content->manifest->getEnvelope().shards.size() * ShardHeader::kSize + offer.manifest->size() + Downloader::kMaximumTransfers * (ChunkRecord::kHeaderSize + Chunker::kMaximumSize);
    return offer.downloadBytes + overhead + std::max(kSafetyMargin, offer.downloadBytes / 20);
}

void Updater::install(const UpdateOffer& offer, std::function<void(const Downloader::Progress&)> progress, std::function<void(std::optional<Error>)> done) {
    // clang-format off
    jobs.postIo([this, &pool = jobs, owner = std::weak_ptr(alive), store = release->getStore(), offer, progress = std::move(progress), done = std::move(done)]() mutable {
        const std::uint64_t available = store->getAvailableSpace();
        const std::uint64_t required = getRequiredSpace(offer);
        std::optional<Error> failure;
        if (required > available) {
            failure = Error(Error::Code::InsufficientStorage, std::format("The update needs {} bytes of room on the device and {} are free. Free some room and try again.", required, available), {.requiredBytes = required, .availableBytes = available});
        } else {
            try {
                markPending(*store, offer);
            } catch (const Error& error) {
                failure = error;
            }
        }
        pool.postToFrame([this, owner, failure = std::move(failure), offer = std::move(offer), progress = std::move(progress), done = std::move(done)]() mutable {
            if (owner.expired()) {
                return;
            }
            if (failure) {
                done(std::move(failure));
                return;
            }
            download = downloader.fetch(offer.content->shards, offer.missing, [this](const ShardReference& pack) { return locate(pack); }, {
                .onProgress = std::move(progress),
                .onComplete = [this, offer, done = std::move(done)](std::optional<Error> failure) mutable {
                    download.reset();
                    if (failure) {
                        done(std::move(failure));
                        return;
                    }
                    commit(offer, std::move(done));
                },
            });
        });
    });
    // clang-format on
}

// The store keeps the manifest of an install and names it as pending before the first record arrives, so the records of an install that a restart interrupts stay for the next attempt.
void Updater::markPending(LocalStore& store, const UpdateOffer& offer) {
    const Digest id = offer.content->manifest->getId();
    StoreState state = store.getState();
    if (state.pending == id || state.active == id) {
        return;
    }
    store.writeManifest(id, *offer.manifest);
    state.pending = id;
    store.commit(std::move(state));
}

void Updater::commit(const UpdateOffer& offer, std::function<void(std::optional<Error>)> done) const {
    // The manifest and every record go to the disk before the state that names them, so a crash leaves either the earlier content or the new one.
    // clang-format off
    jobs.postIo([&pool = jobs, owner = std::weak_ptr(alive), store = release->getStore(), channelName = channel, offer, done = std::move(done)]() mutable {
        std::optional<Error> failure;
        try {
            const Digest id = offer.content->manifest->getId();
            store->writeManifest(id, *offer.manifest);
            StoreState state = store->getState();
            if (state.active != id) {
                state.previous = state.active;
                state.active = id;
            }
            state.pending = Digest();
            state.channel = channelName;
            state.acceptedGeneration = offer.generation;
            state.acceptedManifest = id;
            store->commit(std::move(state));
        } catch (const Error& error) {
            failure = error;
        }
        pool.postToFrame([owner, failure = std::move(failure), done = std::move(done)]() mutable {
            if (!owner.expired()) {
                done(std::move(failure));
            }
        });
    });
    // clang-format on
}

void Updater::cancel() {
    for (const HttpTransport::Transfer& transfer : transfers) {
        transfer.cancel();
    }
    if (download) {
        downloader.cancel(*download);
    }
}

void Updater::collect(std::function<void(Collection, std::optional<Error>)> done) {
    // clang-format off
    jobs.postIo([&pool = jobs, owner = std::weak_ptr(alive), releasePackage = release, done = std::move(done)]() mutable {
        Collection collection;
        std::optional<Error> failure;
        try {
            const std::shared_ptr<LocalStore>& store = releasePackage->getStore();
            const StoreState state = store->getState();
            std::unordered_set<Digest, Digest::Hash> live;
            std::set<Digest> manifests;
            std::set<Digest> shards;
            const auto keep = [&live, &shards](const ReleasePackage::Content& content) {
                for (std::uint64_t index = 0; index < content.catalog->getChunkCount(); ++index) {
                    live.insert(content.catalog->getChunk(index).storedId);
                }
                for (const ShardReference& shard : content.manifest->getEnvelope().shards) {
                    shards.insert(shard.shardId);
                }
            };
            keep(*releasePackage->getBundledContent());
            for (const Digest& id : {state.active, state.previous, state.pending}) {
                if (id.isZero()) {
                    continue;
                }
                if (std::optional<std::vector<std::uint8_t>> bytes = store->readManifest(id)) {
                    keep(*releasePackage->load(std::move(*bytes)));
                    manifests.insert(id);
                }
            }
            const LocalStore::Collection collected = store->collect(live, manifests, shards);
            collection = {.removedRecords = collected.removedRecords, .freedBytes = collected.freedBytes};
        } catch (const Error& error) {
            failure = error;
        }
        pool.postToFrame([owner, collection, failure = std::move(failure), done = std::move(done)]() mutable {
            if (!owner.expired()) {
                done(collection, std::move(failure));
            }
        });
    });
    // clang-format on
}

} // namespace haylen::content
