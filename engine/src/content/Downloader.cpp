#include "content/Downloader.hpp"

#include <monocypher.h>

#include <algorithm>
#include <format>
#include <unordered_set>

#include "content/RangePlanner.hpp"
#include "content/ShardIndexFile.hpp"
#include "content/format/ChunkRecord.hpp"
#include "content/format/ShardHeader.hpp"
#include "haylen/core/JobSystem.hpp"

namespace haylen::content {

Downloader::Downloader(core::JobSystem& jobSystem, HttpTransport& httpTransport, std::shared_ptr<LocalStore> localStore) : jobs(jobSystem), transport(httpTransport), store(std::move(localStore)) {
    for (PartialRecord& partial : store->listPartials()) {
        const auto key = std::make_pair(partial.shardId, partial.recordOffset);
        partials.insert_or_assign(key, std::move(partial));
    }
}

Downloader::~Downloader() {
    for (const std::shared_ptr<Transfer>& transfer : activeTransfers) {
        transfer->abandoned = true;
        transfer->handle.cancel();
    }
}

std::vector<Catalog::Chunk> Downloader::findMissing(const ShardSet& shards, std::span<const Catalog::Chunk> chunks) {
    std::unordered_set<Digest, Digest::Hash> listed;
    std::vector<Catalog::Chunk> missing;
    for (const Catalog::Chunk& chunk : chunks) {
        if (listed.insert(chunk.storedId).second && !shards.hasChunk(chunk)) {
            missing.push_back(chunk);
        }
    }
    return missing;
}

std::uint64_t Downloader::measure(std::span<const Catalog::Chunk> chunks, const LocalStore& store) {
    std::uint64_t bytes = 0;
    for (const Catalog::Chunk& chunk : chunks) {
        if (!store.holds(chunk.storedId)) {
            bytes += ChunkRecord::kHeaderSize + chunk.encodedSize;
        }
    }
    return bytes;
}

std::uint64_t Downloader::fetch(std::shared_ptr<const ShardSet> shardSet, std::span<const Catalog::Chunk> chunks, const Locator& locate, Callbacks callbacks, Priority priority, Use use) {
    const std::uint64_t id = nextRequest++;
    Request& request = requests[id];
    request.callbacks = std::move(callbacks);
    request.priority = priority;

    std::set<Digest> touched;
    std::vector<Digest> held;
    for (const Catalog::Chunk& chunk : chunks) {
        if (use == Use::Read) {
            request.records.push_back(chunk.storedId);
        }
        if (request.waiting.contains(chunk.storedId) || store->contains(chunk.storedId) || (use == Use::Keep && store->holds(chunk.storedId))) {
            continue;
        }
        request.waiting.insert(chunk.storedId);

        // A record that the store holds but cannot read yet, as on the web, comes in from the store instead of the network, so it downloads no byte.
        const bool inStore = store->holds(chunk.storedId);
        if (!inStore) {
            request.progress.totalBytes += ChunkRecord::kHeaderSize + chunk.encodedSize;
        }

        const ShardReference& reference = shardSet->getReferences()[chunk.shard];
        auto [found, added] = wanted.try_emplace(chunk.storedId);
        found->second.requests.push_back(id);
        if (!added) {
            continue;
        }
        found->second.chunk = chunk;
        found->second.shard = reference.shardId;
        auto [shard, created] = shards.try_emplace(reference.shardId);
        if (created) {
            shard->second.reference = reference;
            shard->second.url = locate(reference);
            shard->second.keys = std::shared_ptr<const KeyRing>(shardSet, &shardSet->getKeys());
        }

        if (inStore) {
            found->second.transferring = true;
            held.push_back(chunk.storedId);
            continue;
        }
        shard->second.queued.push_back(chunk.storedId);
        touched.insert(reference.shardId);
    }
    if (!held.empty()) {
        loadHeld(std::move(held));
    }

    if (request.waiting.empty()) {
        complete(id, std::nullopt);
        return id;
    }
    for (const Digest& shardId : touched) {
        Shard& shard = shards.at(shardId);
        if (shard.index) {
            plan(shard);
        } else if (!shard.indexing) {
            indexShard(shard);
        }
    }
    pump();
    return id;
}

void Downloader::loadHeld(std::vector<Digest> storedIds) {
    // clang-format off
    store->load(storedIds, [this, &pool = jobs, owner = std::weak_ptr(alive), storedIds](bool) {
        pool.postToFrame([this, owner, storedIds] {
            if (owner.expired()) {
                return;
            }

            // A record the store lost on the way downloads like any other.
            std::set<Digest> touched;
            for (const Digest& storedId : storedIds) {
                const auto found = wanted.find(storedId);
                if (found == wanted.end()) {
                    continue;
                }
                if (store->contains(storedId)) {
                    stored(storedId, std::nullopt);
                    continue;
                }
                found->second.transferring = false;
                for (const std::uint64_t id : found->second.requests) {
                    requests.at(id).progress.totalBytes += ChunkRecord::kHeaderSize + found->second.chunk.encodedSize;
                }
                shards.at(found->second.shard).queued.push_back(storedId);
                touched.insert(found->second.shard);
            }
            for (const Digest& shardId : touched) {
                Shard& shard = shards.at(shardId);
                if (shard.index) {
                    plan(shard);
                } else if (!shard.indexing) {
                    indexShard(shard);
                }
            }
            pump();
        });
    });
    // clang-format on
}

void Downloader::indexShard(Shard& shard) {
    shard.indexing = true;
    // clang-format off
    jobs.postIo([this, &pool = jobs, owner = std::weak_ptr(alive), localStore = store, reference = shard.reference, keys = shard.keys] {
        std::shared_ptr<const ShardReader> index;
        try {
            if (std::optional<std::vector<std::uint8_t>> bytes = localStore->readShardIndex(reference.shardId)) {
                index = std::make_shared<const ShardReader>(std::make_unique<ShardIndexFile>(std::move(*bytes), reference), reference, *keys);
            }
        } catch (const Error&) {
        }
        pool.postToFrame([this, owner, reference, index] {
            if (owner.expired()) {
                return;
            }
            Shard& indexed = shards.at(reference.shardId);
            if (index) {
                indexed.index = index;
                plan(indexed);
                pump();
                return;
            }

            // The header comes first, since it says where the index lies at the end of the shard.
            fetchRange(indexed.url, 0, ShardHeader::kSize, [this, reference](std::vector<std::uint8_t> header, std::optional<Error> failure) {
                if (failure) {
                    failShard(reference.shardId, *failure);
                    return;
                }
                std::optional<ShardHeader> parsed;
                try {
                    parsed = ShardHeader::parse(std::span(header).first<ShardHeader::kSize>(), reference.fileSize);
                } catch (const Error& error) {
                    failShard(reference.shardId, error);
                    return;
                }
                fetchRange(shards.at(reference.shardId).url, parsed->indexOffset, parsed->indexSize, [this, reference, header = std::move(header)](std::vector<std::uint8_t> indexBytes, std::optional<Error> indexFailure) mutable {
                    if (indexFailure) {
                        failShard(reference.shardId, *indexFailure);
                        return;
                    }
                    header.insert(header.end(), indexBytes.begin(), indexBytes.end());
                    receiveIndex(reference.shardId, std::move(header));
                });
            });
        });
    });
    // clang-format on
}

void Downloader::receiveIndex(const Digest& shardId, std::vector<std::uint8_t> bytes) {
    const Shard& shard = shards.at(shardId);
    // clang-format off
    jobs.post([this, &pool = jobs, owner = std::weak_ptr(alive), localStore = store, reference = shard.reference, keys = shard.keys, bytes = std::move(bytes)]() mutable {
        std::shared_ptr<const ShardReader> index;
        std::optional<Error> failure;
        try {
            index = std::make_shared<const ShardReader>(std::make_unique<ShardIndexFile>(bytes, reference), reference, *keys);
            localStore->writeShardIndex(reference.shardId, bytes);
        } catch (const Error& error) {
            failure = error;
        }
        pool.postToFrame([this, owner, shardId = reference.shardId, index, failure] {
            if (owner.expired()) {
                return;
            }
            if (failure) {
                failShard(shardId, *failure);
                return;
            }
            Shard& indexed = shards.at(shardId);
            indexed.index = index;
            plan(indexed);
            pump();
        });
    });
    // clang-format on
}

void Downloader::fetchRange(const std::string& url, std::uint64_t offset, std::uint64_t size, std::function<void(std::vector<std::uint8_t>, std::optional<Error>)> done) {
    auto bytes = std::make_shared<std::vector<std::uint8_t>>();
    bytes->reserve(static_cast<std::size_t>(size));
    ++statistics.requests;
    // clang-format off
    transport.fetch({.url = url, .offset = offset, .size = size}, {
        .onStart = [](const HttpTransport::Start&) {},
        .onBytes = [this, owner = std::weak_ptr(alive), bytes](std::span<const std::uint8_t> chunk) {
            if (!owner.expired()) {
                statistics.downloadedBytes += chunk.size();
                bytes->insert(bytes->end(), chunk.begin(), chunk.end());
            }
        },
        .onComplete = [owner = std::weak_ptr(alive), bytes, done = std::move(done)](std::optional<Error> failure) {
            if (!owner.expired()) {
                done(std::move(*bytes), std::move(failure));
            }
        },
    });
    // clang-format on
}

void Downloader::failShard(const Digest& shardId, const Error& error) {
    Shard& shard = shards.at(shardId);
    shard.indexing = false;
    const std::vector<Digest> queued = std::exchange(shard.queued, {});
    for (const Digest& storedId : queued) {
        fail(storedId, error);
    }
    shards.erase(shardId);
    dropUnwanted();
}

void Downloader::plan(Shard& shard) {
    std::vector<std::pair<Digest, ShardIndex::Entry>> records;
    for (const Digest& storedId : std::exchange(shard.queued, {})) {
        if (!wanted.contains(storedId)) {
            continue;
        }
        const ShardIndex::Entry* entry = shard.index->getIndex().find(storedId);
        if (entry == nullptr) {
            fail(storedId, Error(Error::Code::MissingChunk, "The shard \"" + shard.reference.shardId.toHex() + "\" does not hold the chunk \"" + storedId.toHex() + "\" that its catalog places in it."));
            continue;
        }
        records.emplace_back(storedId, *entry);
        wanted.at(storedId).transferring = true;
    }
    std::ranges::sort(records, {}, [](const auto& record) { return record.second.recordOffset; });

    // A record with staged bytes starts a range of its own, so its download goes on from where it stopped.
    std::size_t begin = 0;
    while (begin < records.size()) {
        std::size_t end = begin + 1;
        while (end < records.size() && !partials.contains({shard.reference.shardId, records[end].second.recordOffset})) {
            ++end;
        }
        std::vector<RangePlanner::Span> spans;
        spans.reserve(end - begin);
        for (std::size_t index = begin; index < end; ++index) {
            spans.push_back({.offset = records[index].second.recordOffset, .size = ChunkRecord::kHeaderSize + records[index].second.encodedSize});
        }
        for (const RangePlanner::Range& range : RangePlanner::plan(spans)) {
            auto transfer = std::make_shared<Transfer>();
            transfer->shard = shard.reference.shardId;
            const auto first = records.begin() + static_cast<std::ptrdiff_t>(begin + range.first);
            transfer->records.assign(first, first + static_cast<std::ptrdiff_t>(range.count));
            waitingTransfers.push_back(std::move(transfer));
        }
        begin = end;
    }
}

bool Downloader::isForeground(const Transfer& transfer) const {
    // clang-format off
    return std::any_of(transfer.records.begin() + static_cast<std::ptrdiff_t>(transfer.current), transfer.records.end(), [this](const auto& record) {
        const auto found = wanted.find(record.first);
        return found != wanted.end() && std::ranges::any_of(found->second.requests, [this](std::uint64_t id) { return requests.at(id).priority == Priority::Foreground; });
    });
    // clang-format on
}

void Downloader::pump() {
    while (activeTransfers.size() < kMaximumTransfers && !waitingTransfers.empty() && verifyingBytes + store->getUnwrittenBytes() <= kMaximumHeldBytes) {
        // The foreground goes first, and background work takes a request only while nothing in the foreground waits and it holds fewer than its share.
        auto next = std::ranges::find_if(waitingTransfers, [this](const std::shared_ptr<Transfer>& waiting) { return isForeground(*waiting); });
        if (next == waitingTransfers.end()) {
            const auto background = std::ranges::count_if(activeTransfers, [this](const std::shared_ptr<Transfer>& active) { return !isForeground(*active); });
            if (static_cast<std::size_t>(background) >= kMaximumBackgroundTransfers) {
                return;
            }
            next = waitingTransfers.begin();
        }
        std::shared_ptr<Transfer> transfer = std::move(*next);
        waitingTransfers.erase(next);
        // clang-format off
        const bool needed = std::ranges::any_of(transfer->records, [this](const auto& record) {
            const auto found = wanted.find(record.first);
            return found != wanted.end() && !found->second.requests.empty();
        });
        // clang-format on
        if (needed) {
            start(std::move(transfer));
            continue;
        }
        for (const auto& [storedId, entry] : transfer->records) {
            if (const auto found = wanted.find(storedId); found != wanted.end()) {
                found->second.transferring = false;
            }
        }
        dropUnwanted();
    }
}

void Downloader::start(std::shared_ptr<Transfer> transfer) {
    const Shard& shard = shards.at(transfer->shard);
    const ShardIndex::Entry& first = transfer->records.front().second;
    const ShardIndex::Entry& last = transfer->records.back().second;
    const std::uint64_t offset = first.recordOffset;
    const std::uint64_t size = last.recordOffset + ChunkRecord::kHeaderSize + last.encodedSize - offset;

    // A record whose download stopped before goes on from its staged bytes, and its range starts with it.
    HttpTransport::Request request{.url = shard.url, .offset = offset, .size = size};
    if (const auto partial = partials.find({transfer->shard, offset}); partial != partials.end()) {
        request.held = partial->second.bytes.size();
        request.validator = partial->second.validator;
        transfer->buffer = partial->second.bytes;
    }
    transfer->position = offset + request.held;
    activeTransfers.push_back(transfer);
    ++statistics.requests;

    // clang-format off
    transfer->handle = transport.fetch(std::move(request), {
        .onStart = [this, owner = std::weak_ptr(alive), transfer, offset](const HttpTransport::Start& started) {
            if (owner.expired()) {
                return;
            }
            transfer->validator = started.validator;
            if (started.resumed) {
                ++statistics.resumedRecords;
                return;
            }
            transfer->buffer.clear();
            transfer->position = offset;
        },
        .onBytes = [this, owner = std::weak_ptr(alive), transfer](std::span<const std::uint8_t> bytes) {
            if (!owner.expired()) {
                receive(*transfer, bytes);
            }
        },
        .onComplete = [this, owner = std::weak_ptr(alive), transfer](std::optional<Error> failure) {
            if (!owner.expired()) {
                finishTransfer(transfer, std::move(failure));
            }
        },
    });
    // clang-format on
}

void Downloader::receive(Transfer& transfer, std::span<const std::uint8_t> bytes) {
    statistics.downloadedBytes += bytes.size();
    while (!bytes.empty() && transfer.current < transfer.records.size()) {
        const auto& [storedId, entry] = transfer.records[transfer.current];
        const std::uint64_t end = entry.recordOffset + ChunkRecord::kHeaderSize + entry.encodedSize;

        // The bytes between two records of a range belong to records nobody asked for.
        if (transfer.position < entry.recordOffset) {
            const auto skipped = static_cast<std::size_t>(std::min<std::uint64_t>(entry.recordOffset - transfer.position, bytes.size()));
            transfer.position += skipped;
            bytes = bytes.subspan(skipped);
            continue;
        }

        const auto taken = static_cast<std::size_t>(std::min<std::uint64_t>(end - transfer.position, bytes.size()));
        transfer.buffer.insert(transfer.buffer.end(), bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(taken));
        transfer.position += taken;
        bytes = bytes.subspan(taken);
        if (const auto found = wanted.find(storedId); found != wanted.end()) {
            for (const std::uint64_t id : found->second.requests) {
                Request& request = requests.at(id);
                request.progress.receivedBytes += taken;
                request.changed = true;
            }
        }
        if (transfer.position == end) {
            verify(transfer.shard, storedId, std::exchange(transfer.buffer, {}));
            ++transfer.current;
        }
    }
}

void Downloader::finishTransfer(const std::shared_ptr<Transfer>& transfer, std::optional<Error> failure) {
    std::erase(activeTransfers, transfer);
    if (failure && !transfer->abandoned && transfer->current < transfer->records.size()) {
        // The first bytes of the record that stopped stay for a resume, as long as they are worth a request of their own.
        const auto& [storedId, entry] = transfer->records[transfer->current];
        if (transfer->buffer.size() >= kMinimumPartialSize && !transfer->validator.empty()) {
            PartialRecord partial{.shardId = transfer->shard, .recordOffset = entry.recordOffset, .recordSize = ChunkRecord::kHeaderSize + entry.encodedSize, .validator = transfer->validator, .bytes = std::move(transfer->buffer)};
            partials.insert_or_assign({transfer->shard, entry.recordOffset}, partial);
            // A device that refuses the staged bytes only costs the resume, and the record downloads whole next time.
            // clang-format off
            jobs.postIo([localStore = store, partial = std::move(partial)] {
                try {
                    localStore->writePartial(partial);
                } catch (const Error&) {
                }
            });
            // clang-format on
        }
        for (std::size_t index = transfer->current; index < transfer->records.size(); ++index) {
            fail(transfer->records[index].first, *failure);
        }
    }
    // A record of a transfer that stopped because nobody wanted it may have found a request since, which downloads it again.
    std::set<Digest> touched;
    for (std::size_t index = transfer->current; index < transfer->records.size(); ++index) {
        const auto found = wanted.find(transfer->records[index].first);
        if (found == wanted.end()) {
            continue;
        }
        found->second.transferring = false;
        if (!found->second.requests.empty() && shards.contains(found->second.shard)) {
            shards.at(found->second.shard).queued.push_back(found->first);
            touched.insert(found->second.shard);
        }
    }
    for (const Digest& shardId : touched) {
        Shard& shard = shards.at(shardId);
        if (shard.index) {
            plan(shard);
        } else if (!shard.indexing) {
            indexShard(shard);
        }
    }
    dropUnwanted();
    pump();
}

void Downloader::verify(const Digest& shardId, const Digest& storedId, std::vector<std::uint8_t> record) {
    const auto found = wanted.find(storedId);
    if (found == wanted.end()) {
        return;
    }
    const Shard& shard = shards.at(shardId);
    const ShardIndex::Entry entry = *shard.index->getIndex().find(storedId);
    if (partials.erase({shardId, entry.recordOffset}) > 0) {
        jobs.postIo([localStore = store, shardId, offset = entry.recordOffset] { localStore->removePartial(shardId, offset); });
    }

    // The record is checked against the signed catalog and the index of its shard and opened on the task pool, and only then written to the store on the I/O pool.
    const std::uint64_t size = record.size();
    verifyingBytes += size;
    // clang-format off
    jobs.post([this, &pool = jobs, owner = std::weak_ptr(alive), localStore = store, chunk = found->second.chunk, entry, keys = shard.keys, keyId = shard.index->getHeader().keyId, size, record = std::move(record)]() mutable {
        std::optional<Error> failure;
        try {
            const ChunkRecord header = ChunkRecord::parse(std::span(record).first<ChunkRecord::kHeaderSize>());
            if (!header.describes(chunk) || header.codec != entry.codec || header.profile != entry.profile || header.plainSize != entry.plainSize) {
                throw Error(Error::Code::CorruptChunk, "The downloaded record of the chunk \"" + chunk.storedId.toHex() + "\" does not match its catalog and the index of its shard.");
            }
            std::vector<std::uint8_t> ciphertext(record.begin() + ChunkRecord::kHeaderSize, record.end());
            std::vector<std::uint8_t> plain;
            header.open(*keys->get(keyId), ciphertext, plain);
            crypto_wipe(plain.data(), plain.size());
        } catch (const Error& error) {
            failure = error;
        }
        if (failure) {
            pool.postToFrame([this, owner, storedId = chunk.storedId, size, failure] {
                if (!owner.expired()) {
                    verifyingBytes -= size;
                    stored(storedId, failure);
                }
            });
            return;
        }
        pool.postIo([this, &pool, owner, localStore, storedId = chunk.storedId, keyId, size, record = std::move(record)] {
            std::optional<Error> writeFailure;
            try {
                localStore->addRecord(keyId, record);
            } catch (const Error& error) {
                writeFailure = error;
            }
            pool.postToFrame([this, owner, storedId, size, writeFailure] {
                if (!owner.expired()) {
                    verifyingBytes -= size;
                    stored(storedId, writeFailure);
                }
            });
        });
    });
    // clang-format on
}

void Downloader::stored(const Digest& storedId, std::optional<Error> failure) {
    if (failure) {
        fail(storedId, *failure);
        return;
    }
    const auto found = wanted.find(storedId);
    if (found == wanted.end()) {
        return;
    }
    ++statistics.storedRecords;
    const std::vector<std::uint64_t> waiting = std::move(found->second.requests);
    wanted.erase(found);
    for (const std::uint64_t id : waiting) {
        Request& request = requests.at(id);
        request.waiting.erase(storedId);
        if (request.waiting.empty()) {
            complete(id, std::nullopt);
        }
    }
}

// The attempt at the record is over, so the record stops being wanted and a later request downloads it again.
void Downloader::fail(const Digest& storedId, const Error& error) {
    const auto found = wanted.find(storedId);
    if (found == wanted.end()) {
        return;
    }
    Error failure = error;
    Error::Details& details = failure.getDetails();
    details.packId = found->second.shard.toHex();
    details.contentId = found->second.chunk.contentId.toHex();
    details.storedId = storedId.toHex();
    const std::vector<std::uint64_t> waiting = std::move(found->second.requests);
    wanted.erase(found);
    for (const std::uint64_t id : waiting) {
        complete(id, failure);
    }
}

void Downloader::cancel(std::uint64_t request) {
    if (requests.contains(request)) {
        complete(request, Error(Error::Code::NetworkCancelled, "The download was cancelled."));
        dropUnwanted();
        pump();
    }
}

void Downloader::complete(std::uint64_t id, std::optional<Error> failure) {
    const auto found = requests.find(id);
    if (found == requests.end()) {
        return;
    }
    Request request = std::move(found->second);
    requests.erase(found);
    for (const Digest& storedId : request.waiting) {
        if (const auto record = wanted.find(storedId); record != wanted.end()) {
            std::erase(record->second.requests, id);
        }
    }

    // The callbacks run in a later turn of the loop, so callers never meet them inside their own call.
    // clang-format off
    auto finish = [&pool = jobs, owner = std::weak_ptr(alive), callbacks = std::move(request.callbacks), progress = request.progress](std::optional<Error> result) mutable {
        pool.postToFrame([owner, callbacks = std::move(callbacks), progress, result = std::move(result)] {
            if (owner.expired()) {
                return;
            }
            if (callbacks.onProgress && !result) {
                callbacks.onProgress({.receivedBytes = progress.totalBytes, .totalBytes = progress.totalBytes});
            }
            callbacks.onComplete(result);
        });
    };
    if (failure || request.records.empty()) {
        finish(std::move(failure));
    } else {
        store->load(request.records, [finish = std::move(finish)](bool ok) mutable {
            finish(ok ? std::nullopt : std::optional(Error(Error::Code::CacheCorrupt, "The content store could not read in the downloaded content. Try again.")));
        });
    }
    // clang-format on
    dropUnwanted();
}

void Downloader::dropUnwanted() {
    std::erase_if(wanted, [](const auto& record) { return record.second.requests.empty() && !record.second.transferring; });

    // A request whose records nobody waits for anymore stops, so it downloads nothing that no one needs.
    for (const std::shared_ptr<Transfer>& transfer : activeTransfers) {
        // clang-format off
        const bool needed = std::any_of(transfer->records.begin() + static_cast<std::ptrdiff_t>(transfer->current), transfer->records.end(), [this](const auto& record) {
            const auto found = wanted.find(record.first);
            return found != wanted.end() && !found->second.requests.empty();
        });
        // clang-format on
        if (!needed && !transfer->abandoned) {
            transfer->abandoned = true;
            transfer->handle.cancel();
        }
    }
}

void Downloader::update() {
    std::vector<std::pair<std::function<void(const Progress&)>, Progress>> reports;
    for (auto& [id, request] : requests) {
        if (request.changed && request.callbacks.onProgress) {
            reports.emplace_back(request.callbacks.onProgress, request.progress);
        }
        request.changed = false;
    }
    for (const auto& [report, progress] : reports) {
        report(progress);
    }
    if (!waitingTransfers.empty()) {
        pump();
    }
}

} // namespace haylen::content
