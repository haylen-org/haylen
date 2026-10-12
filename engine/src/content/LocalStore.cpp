#include "content/LocalStore.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <map>
#include <utility>

#include "content/format/ChunkRecord.hpp"
#include "content/format/StoreEntry.hpp"
#include "haylen/content/Error.hpp"

namespace haylen::content {

LocalStore::LocalStore(std::unique_ptr<StoreFiles> storeFiles, const Digest& application, std::uint64_t appBuild, std::uint64_t segmentTarget) : files(std::move(storeFiles)), state(readState(application, appBuild)), target(segmentTarget) {
    readJournal();
    removeLeftovers();
}

std::string LocalStore::getSegmentName(std::uint32_t segment) {
    return std::format("{}/{:08}.hseg", kSegmentFolder, segment);
}

std::string LocalStore::getJournalName(std::uint64_t journal) {
    return std::format("journal-{}", journal);
}

std::string LocalStore::getStateName(std::uint64_t sequence) {
    return sequence % 2 == 0 ? "state-a" : "state-b";
}

std::string LocalStore::getPartialName(const Digest& shard, std::uint64_t recordOffset) {
    return std::format("{}/{}-{}.hpart", kStagingFolder, shard.toHex(), recordOffset);
}

StoreState LocalStore::readState(const Digest& application, std::uint64_t appBuild) const {
    // The slot with the highest sequence that is whole wins, so a torn write of one slot leaves the other.
    std::optional<StoreState> newest;
    for (const std::uint64_t slot : {std::uint64_t{0}, std::uint64_t{1}}) {
        const std::optional<std::vector<std::uint8_t>> bytes = files->read(getStateName(slot));
        if (!bytes) {
            continue;
        }
        try {
            StoreState read = StoreState::parse(*bytes);
            if (!newest || read.sequence > newest->sequence) {
                newest = std::move(read);
            }
        } catch (const Error&) {}
    }
    if (!newest || newest->application != application) {
        return {.application = application, .appBuild = appBuild};
    }
    if (newest->appBuild != appBuild) {
        newest->appBuild = appBuild;
        newest->active = Digest();
        newest->previous = Digest();
    }
    return *newest;
}

void LocalStore::readJournal() {
    const std::string name = getJournalName(state.journal);
    std::vector<std::uint8_t> journal = files->read(name).value_or(std::vector<std::uint8_t>{});

    // A crash may tear the last entry, which the next entry must not follow.
    if (journal.size() % StoreEntry::kSize != 0) {
        journal.resize(journal.size() - journal.size() % StoreEntry::kSize);
        files->truncate(name, journal.size());
    }
    for (std::size_t offset = 0; offset < journal.size(); offset += StoreEntry::kSize) {
        const std::optional<StoreEntry> entry = StoreEntry::parse(std::span(journal).subspan(offset).first<StoreEntry::kSize>());
        if (!entry) {
            continue;
        }
        if (entry->removed) {
            records.erase(entry->storedId);
            continue;
        }
        records.insert_or_assign(entry->storedId, Location{.segment = entry->segment, .offset = entry->offset, .size = entry->size, .keyId = entry->keyId});
    }

    // A segment that a platform evicted or that lost its end takes its records with it, which downloads bring back.
    std::map<std::uint32_t, std::uint64_t> sizes;
    // clang-format off
    std::erase_if(records, [this, &sizes](const auto& item) {
        const Location& location = item.second;
        const auto [found, added] = sizes.try_emplace(location.segment, 0);
        if (added) {
            found->second = files->getSize(getSegmentName(location.segment));
        }
        return location.offset < kSegmentHeader.size() || location.offset + location.size > found->second;
    });
    // clang-format on
    for (const auto& [segment, size] : sizes) {
        if (size > 0) {
            appendSegment = std::max(appendSegment, segment);
        }
    }
}

void LocalStore::removeLeftovers() {
    std::set<std::uint32_t> used;
    for (const auto& [storedId, location] : records) {
        used.insert(location.segment);
    }
    for (const std::string& name : files->list(std::string(kSegmentFolder))) {
        std::uint32_t segment = 0;
        const auto [end, error] = std::from_chars(name.data(), name.data() + name.size(), segment);
        if (error != std::errc() || std::string_view(end) != ".hseg" || !used.contains(segment)) {
            files->remove(std::string(kSegmentFolder) + "/" + name);
        }
    }
    for (const std::string& name : files->list("")) {
        if (name.starts_with("journal-") && name != getJournalName(state.journal)) {
            files->remove(name);
        }
    }
}

StoreState LocalStore::getState() const {
    std::shared_lock lock(mutex);
    return state;
}

void LocalStore::commit(StoreState next) {
    std::scoped_lock write(writeMutex);
    flush();
    writeState(std::move(next));
}

void LocalStore::writeState(StoreState next) {
    next.sequence = state.sequence + 1;
    files->write(getStateName(next.sequence), next.write());
    std::unique_lock lock(mutex);
    state = std::move(next);
}

bool LocalStore::contains(const Digest& storedId) const {
    std::shared_lock lock(mutex);
    const auto found = records.find(storedId);
    return found != records.end() && files->isResident(getSegmentName(found->second.segment), found->second.offset, found->second.size);
}

bool LocalStore::holds(const Digest& storedId) const {
    std::shared_lock lock(mutex);
    return records.contains(storedId);
}

std::vector<StoreFiles::Range> LocalStore::getRanges(const std::vector<Digest>& storedIds) const {
    std::shared_lock lock(mutex);
    std::vector<StoreFiles::Range> ranges;
    for (const Digest& storedId : storedIds) {
        if (const auto found = records.find(storedId); found != records.end()) {
            ranges.push_back({.name = getSegmentName(found->second.segment), .offset = found->second.offset, .size = found->second.size});
        }
    }
    return ranges;
}

void LocalStore::load(const std::vector<Digest>& storedIds, std::function<void(bool)> done) {
    files->load(getRanges(storedIds), std::move(done));
}

void LocalStore::pin(const std::vector<Digest>& storedIds) {
    files->pin(getRanges(storedIds));
}

bool LocalStore::readRecord(const Digest& storedId, std::vector<std::uint8_t>& record, Digest& keyId) const {
    // The lock stays shared during the read, so a collection never removes a segment under a reader.
    std::shared_lock lock(mutex);
    const auto found = records.find(storedId);
    if (found == records.end()) {
        return false;
    }
    const Location& location = found->second;
    if (!files->isResident(getSegmentName(location.segment), location.offset, location.size)) {
        return false;
    }
    record.resize(static_cast<std::size_t>(location.size));
    files->readRange(getSegmentName(location.segment), location.offset, record);
    keyId = location.keyId;
    return true;
}

void LocalStore::addRecord(const Digest& keyId, std::span<const std::uint8_t> record) {
    const ChunkRecord header = ChunkRecord::parse(record.first<ChunkRecord::kHeaderSize>());
    std::scoped_lock write(writeMutex);
    if (holds(header.storedId)) {
        return;
    }
    const Location location = appendRecord(keyId, record);
    const StoreEntry entry{.storedId = header.storedId, .keyId = keyId, .segment = location.segment, .offset = location.offset, .size = location.size};
    files->append(getJournalName(state.journal), entry.write());
    dirty.insert(getJournalName(state.journal));
    std::unique_lock lock(mutex);
    records.insert_or_assign(header.storedId, location);
}

LocalStore::Location LocalStore::appendRecord(const Digest& keyId, std::span<const std::uint8_t> record) {
    std::string name = getSegmentName(appendSegment);
    std::uint64_t size = files->getSize(name);
    if (size > kSegmentHeader.size() && size + record.size() > target) {
        ++appendSegment;
        name = getSegmentName(appendSegment);
        size = files->getSize(name);
    }
    if (size == 0) {
        files->append(name, kSegmentHeader);
    }
    const std::uint64_t offset = files->append(name, record);
    dirty.insert(name);
    return {.segment = appendSegment, .offset = offset, .size = record.size(), .keyId = keyId};
}

void LocalStore::removeRecord(const Digest& storedId) {
    std::scoped_lock write(writeMutex);
    if (!holds(storedId)) {
        return;
    }
    const StoreEntry entry{.storedId = storedId, .removed = true};
    files->append(getJournalName(state.journal), entry.write());
    dirty.insert(getJournalName(state.journal));
    std::unique_lock lock(mutex);
    records.erase(storedId);
}

void LocalStore::flush() {
    for (const std::string& name : dirty) {
        files->sync(name);
    }
    dirty.clear();
}

std::optional<std::vector<std::uint8_t>> LocalStore::readManifest(const Digest& id) const {
    return files->read(std::format("{}/{}.hmanifest", kManifestFolder, id.toHex()));
}

void LocalStore::writeManifest(const Digest& id, std::span<const std::uint8_t> bytes) {
    std::scoped_lock write(writeMutex);
    files->write(std::format("{}/{}.hmanifest", kManifestFolder, id.toHex()), bytes);
}

std::optional<std::vector<std::uint8_t>> LocalStore::readShardIndex(const Digest& shard) const {
    return files->read(std::format("{}/{}.hidx", kIndexFolder, shard.toHex()));
}

void LocalStore::writeShardIndex(const Digest& shard, std::span<const std::uint8_t> bytes) {
    std::scoped_lock write(writeMutex);
    files->write(std::format("{}/{}.hidx", kIndexFolder, shard.toHex()), bytes);
}

std::optional<PartialRecord> LocalStore::readPartial(const Digest& shard, std::uint64_t recordOffset) const {
    const std::optional<std::vector<std::uint8_t>> bytes = files->read(getPartialName(shard, recordOffset));
    if (!bytes) {
        return std::nullopt;
    }
    try {
        PartialRecord partial = PartialRecord::parse(*bytes);
        if (partial.shardId == shard && partial.recordOffset == recordOffset) {
            return partial;
        }
    } catch (const Error&) {}
    return std::nullopt;
}

void LocalStore::writePartial(const PartialRecord& partial) {
    std::scoped_lock write(writeMutex);
    files->write(getPartialName(partial.shardId, partial.recordOffset), partial.write());
}

std::vector<PartialRecord> LocalStore::listPartials() const {
    std::vector<PartialRecord> partials;
    for (const std::string& name : files->list(std::string(kStagingFolder))) {
        const std::optional<std::vector<std::uint8_t>> bytes = files->read(std::string(kStagingFolder) + "/" + name);
        try {
            if (bytes) {
                partials.push_back(PartialRecord::parse(*bytes));
            }
        } catch (const Error&) {}
    }
    return partials;
}

void LocalStore::removePartial(const Digest& shard, std::uint64_t recordOffset) {
    std::scoped_lock write(writeMutex);
    files->remove(getPartialName(shard, recordOffset));
}

LocalStore::Collection LocalStore::collect(const std::unordered_set<Digest, Digest::Hash>& liveRecords, const std::set<Digest>& keptManifests, const std::set<Digest>& keptShards) {
    std::scoped_lock write(writeMutex);
    flush();
    Collection collection;

    // The live bytes of every segment decide which segments stay as they are, which move their live records and which go.
    std::unordered_map<Digest, Location, Digest::Hash> kept;
    std::map<std::uint32_t, std::uint64_t> live;
    std::set<std::uint32_t> unreadable;
    for (const auto& [storedId, location] : records) {
        if (!liveRecords.contains(storedId)) {
            ++collection.removedRecords;
            continue;
        }
        kept.emplace(storedId, location);
        live[location.segment] += location.size;
        if (!files->isResident(getSegmentName(location.segment), location.offset, location.size)) {
            unreadable.insert(location.segment);
        }
    }
    const std::uint32_t oldAppend = appendSegment;
    std::set<std::uint32_t> emptied;
    for (const std::string& name : files->list(std::string(kSegmentFolder))) {
        std::uint32_t segment = 0;
        std::from_chars(name.data(), name.data() + name.size(), segment);
        const std::uint64_t size = files->getSize(std::string(kSegmentFolder) + "/" + name);
        // A segment moves its live records only when it can read them now, which the web cannot for records it never read in.
        if (live[segment] == 0 || (segment != oldAppend && live[segment] < size / 2 && !unreadable.contains(segment))) {
            emptied.insert(segment);
            collection.freedBytes += size;
        }
    }
    if (emptied.contains(oldAppend)) {
        ++appendSegment;
    }

    std::vector<std::uint8_t> bytes;
    for (auto& [storedId, location] : kept) {
        if (!emptied.contains(location.segment)) {
            continue;
        }
        bytes.resize(static_cast<std::size_t>(location.size));
        files->readRange(getSegmentName(location.segment), location.offset, bytes);
        location = appendRecord(location.keyId, bytes);
        collection.freedBytes -= std::min(collection.freedBytes, location.size);
        ++collection.movedRecords;
    }
    flush();

    // The new journal lists only the live records, and the new state switches to it before anything is deleted.
    const std::uint64_t journal = state.journal + 1;
    std::vector<std::uint8_t> entries;
    entries.reserve(kept.size() * StoreEntry::kSize);
    for (const auto& [storedId, location] : kept) {
        const StoreEntry::Bytes entry = StoreEntry{.storedId = storedId, .keyId = location.keyId, .segment = location.segment, .offset = location.offset, .size = location.size}.write();
        entries.insert(entries.end(), entry.begin(), entry.end());
    }
    files->write(getJournalName(journal), entries);
    StoreState next = state;
    next.journal = journal;
    writeState(std::move(next));

    std::unique_lock lock(mutex);
    records = std::move(kept);
    files->remove(getJournalName(journal - 1));
    for (const std::uint32_t segment : emptied) {
        files->remove(getSegmentName(segment));
    }
    for (const std::string& name : files->list(std::string(kManifestFolder))) {
        const std::optional<Digest> id = Digest::fromHex(name.substr(0, name.find('.')));
        if (!id || !keptManifests.contains(*id)) {
            files->remove(std::string(kManifestFolder) + "/" + name);
        }
    }
    for (const std::string& folder : {std::string(kIndexFolder), std::string(kStagingFolder)}) {
        for (const std::string& name : files->list(folder)) {
            const std::optional<Digest> shard = Digest::fromHex(name.substr(0, Digest::kSize * 2));
            if (!shard || !keptShards.contains(*shard)) {
                files->remove(folder + "/" + name);
            }
        }
    }
    return collection;
}

LocalStore::Statistics LocalStore::getStatistics() const {
    std::shared_lock lock(mutex);
    Statistics statistics{.records = records.size()};
    std::set<std::uint32_t> segments;
    for (const auto& [storedId, location] : records) {
        statistics.recordBytes += location.size;
        segments.insert(location.segment);
    }
    statistics.segments = segments.size();
    for (const std::uint32_t segment : segments) {
        statistics.segmentBytes += files->getSize(getSegmentName(segment));
    }
    return statistics;
}

std::uint64_t LocalStore::getAvailableSpace() const {
    return files->getAvailableSpace();
}

} // namespace haylen::content
