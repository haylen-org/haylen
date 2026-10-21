#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "content/FolderStoreFiles.hpp"
#include "content/LocalStore.hpp"
#include "content/crypto/ContentKey.hpp"
#include "content/format/ChunkRecord.hpp"
#include "content/format/PartialRecord.hpp"
#include "content/format/RecordSealer.hpp"
#include "content/format/StoreEntry.hpp"
#include "content/format/StoreState.hpp"
#include "haylen/content/Error.hpp"
#include "support/CrashingStoreFiles.hpp"
#include "support/LimitedStoreFiles.hpp"
#include "support/ReleaseFixture.hpp"
#include "support/TemporaryDirectory.hpp"
#include "support/TestFiles.hpp"

namespace haylen::content {

class LocalStoreTest : public ::testing::Test {
  protected:
    static constexpr std::uint64_t kAppBuild = 3;

    [[nodiscard]] std::unique_ptr<LocalStore> open(std::uint64_t appBuild = kAppBuild, std::uint64_t segmentTarget = LocalStore::kSegmentTarget) const {
        return std::make_unique<LocalStore>(std::make_unique<FolderStoreFiles>(folder.getPath()), application, appBuild, segmentTarget);
    }

    // Fills three segments of four records each and returns the records, with a store whose device reports the free room a test gives it.
    [[nodiscard]] std::vector<std::vector<std::uint8_t>> fillSegments(std::unique_ptr<LocalStore>& store, std::uint64_t availableBytes) const {
        std::vector<std::vector<std::uint8_t>> records;
        for (std::uint64_t seed = 0; seed < 12; ++seed) {
            records.push_back(makeRecord(seed));
        }
        store = std::make_unique<LocalStore>(std::make_unique<test::LimitedStoreFiles>(folder.getPath(), availableBytes), application, kAppBuild, 4 * records[0].size() + 16);
        for (const std::vector<std::uint8_t>& record : records) {
            store->addRecord(key.getId(), record);
        }
        return records;
    }

    // The first segment keeps all of its records, the second one of four and the third none.
    [[nodiscard]] static std::unordered_set<Digest, Digest::Hash> keepSomeRecords(const std::vector<std::vector<std::uint8_t>>& records) {
        std::unordered_set<Digest, Digest::Hash> live;
        for (const std::size_t index : {0U, 1U, 2U, 3U, 5U}) {
            live.insert(idOf(records[index]));
        }
        return live;
    }

    // A sealed record of random bytes, as a download brings it.
    [[nodiscard]] std::vector<std::uint8_t> makeRecord(std::uint64_t seed, std::size_t size = 4000) const {
        const std::vector<std::uint8_t> plain = test::TestFiles::randomBytes(size, seed);
        std::vector<std::uint8_t> ciphertext;
        const ChunkRecord record = RecordSealer::seal(key, plain, ChunkRecord::identifyContent(plain), ciphertext);
        const ChunkRecord::Header header = record.serialize();
        std::vector<std::uint8_t> bytes(header.begin(), header.end());
        bytes.insert(bytes.end(), ciphertext.begin(), ciphertext.end());
        return bytes;
    }

    [[nodiscard]] static Digest idOf(const std::vector<std::uint8_t>& record) {
        return ChunkRecord::parse(std::span(record).first<ChunkRecord::kHeaderSize>()).storedId;
    }

    [[nodiscard]] std::size_t countSegments() const {
        std::size_t count = 0;
        for ([[maybe_unused]] const auto& entry : std::filesystem::directory_iterator(folder.getPath() / "segments")) {
            ++count;
        }
        return count;
    }

    test::TemporaryDirectory folder;
    Digest application = Manifest::identifyApplication("com.haylen.tests");
    ContentKey key{test::ReleaseFixture::makeKey(1)};
};

TEST_F(LocalStoreTest, KeepsRecordsAcrossOpenings) {
    const std::vector<std::uint8_t> first = makeRecord(1);
    const std::vector<std::uint8_t> second = makeRecord(2);
    {
        const std::unique_ptr<LocalStore> store = open();
        store->addRecord(key.getId(), first);
        store->addRecord(key.getId(), second);
        store->addRecord(key.getId(), first);
        store->flush();
    }

    const std::unique_ptr<LocalStore> store = open();
    std::vector<std::uint8_t> read;
    Digest keyId;
    ASSERT_TRUE(store->readRecord(idOf(second), read, keyId));
    EXPECT_EQ(read, second);
    EXPECT_EQ(keyId, key.getId());
    EXPECT_TRUE(store->contains(idOf(first)));
    EXPECT_EQ(store->getStatistics().records, 2U);
    EXPECT_FALSE(store->readRecord(idOf(makeRecord(3)), read, keyId));
}

TEST_F(LocalStoreTest, ForgetsRemovedRecords) {
    const std::vector<std::uint8_t> record = makeRecord(1);
    {
        const std::unique_ptr<LocalStore> store = open();
        store->addRecord(key.getId(), record);
        store->removeRecord(idOf(record));
        EXPECT_FALSE(store->contains(idOf(record)));
        store->flush();
    }
    EXPECT_FALSE(open()->contains(idOf(record)));
}

TEST_F(LocalStoreTest, KeepsTheEarlierStateWhenTheNewestSlotTears) {
    const Digest earlier = Digest::of(test::TestFiles::bytes("earlier"));
    const Digest later = Digest::of(test::TestFiles::bytes("later"));
    {
        const std::unique_ptr<LocalStore> store = open();
        StoreState state = store->getState();
        state.active = earlier;
        state.acceptedGeneration = 4;
        store->commit(state);
        state.active = later;
        store->commit(state);
        EXPECT_EQ(store->getState().active, later);
    }

    // The second commit went to the second slot, and a crash that tears it leaves the first one.
    std::filesystem::resize_file(folder.getPath() / "state-a", 20);
    const std::unique_ptr<LocalStore> store = open();
    EXPECT_EQ(store->getState().active, earlier);
    EXPECT_EQ(store->getState().acceptedGeneration, 4U);
}

TEST_F(LocalStoreTest, DropsATornJournalEntryAndAppendsAfterTheWholeOnes) {
    const std::vector<std::uint8_t> first = makeRecord(1);
    const std::vector<std::uint8_t> second = makeRecord(2);
    {
        const std::unique_ptr<LocalStore> store = open();
        store->addRecord(key.getId(), first);
        store->flush();
    }
    std::ofstream(folder.getPath() / "journal-0", std::ios::binary | std::ios::app) << "torn entry";
    {
        const std::unique_ptr<LocalStore> store = open();
        EXPECT_TRUE(store->contains(idOf(first)));
        store->addRecord(key.getId(), second);
        store->flush();
    }

    const std::unique_ptr<LocalStore> store = open();
    EXPECT_TRUE(store->contains(idOf(first)));
    EXPECT_TRUE(store->contains(idOf(second)));
    EXPECT_EQ(std::filesystem::file_size(folder.getPath() / "journal-0"), 2 * StoreEntry::kSize);
}

TEST_F(LocalStoreTest, LosesTheRecordsOfASegmentThatThePlatformEvicted) {
    const std::vector<std::uint8_t> record = makeRecord(1);
    {
        const std::unique_ptr<LocalStore> store = open();
        store->addRecord(key.getId(), record);
        store->flush();
    }
    std::filesystem::remove_all(folder.getPath() / "segments");

    const std::unique_ptr<LocalStore> store = open();
    EXPECT_FALSE(store->contains(idOf(record)));
    store->addRecord(key.getId(), record);
    EXPECT_TRUE(store->contains(idOf(record)));
}

// The store keeps its files open while the app runs, and iOS and tvOS may empty the caches of an app they suspended.
TEST_F(LocalStoreTest, ForgetsTheRecordsOfASegmentThatThePlatformRemovedWhileTheStoreRan) {
    const std::vector<std::uint8_t> first = makeRecord(1);
    const std::vector<std::uint8_t> second = makeRecord(2);
    const std::unique_ptr<LocalStore> store = open();
    store->addRecord(key.getId(), first);
    store->flush();

    std::filesystem::remove_all(folder.getPath() / "segments");
    EXPECT_EQ(store->forgetLostRecords(), 1U);
    EXPECT_FALSE(store->contains(idOf(first)));

    // A record that downloads again lands in the folder, where the next run finds it.
    store->addRecord(key.getId(), second);
    store->flush();
    std::vector<std::uint8_t> read;
    Digest keyId;
    ASSERT_TRUE(store->readRecord(idOf(second), read, keyId));
    EXPECT_EQ(read, second);
    EXPECT_TRUE(open()->contains(idOf(second)));
}

TEST_F(LocalStoreTest, ForgetsWhatARemovedSegmentHeldBeforeItWritesTheSegmentAgain) {
    const std::vector<std::uint8_t> first = makeRecord(1);
    const std::vector<std::uint8_t> second = makeRecord(2);
    const std::vector<std::uint8_t> third = makeRecord(3, 9000);
    const std::unique_ptr<LocalStore> store = open();
    store->addRecord(key.getId(), first);
    store->addRecord(key.getId(), second);
    store->flush();

    // A download that lands before the store looks for what the platform removed writes the segment again, longer than the first record it held.
    std::filesystem::remove_all(folder.getPath() / "segments");
    store->addRecord(key.getId(), third);
    EXPECT_FALSE(store->contains(idOf(first)));
    EXPECT_FALSE(store->contains(idOf(second)));
    EXPECT_EQ(store->forgetLostRecords(), 0U);

    std::vector<std::uint8_t> read;
    Digest keyId;
    ASSERT_TRUE(store->readRecord(idOf(third), read, keyId));
    EXPECT_EQ(read, third);
}

TEST_F(LocalStoreTest, StartsANewBuildFromTheContentItShipsAndKeepsTheGenerationItAccepted) {
    {
        const std::unique_ptr<LocalStore> store = open();
        StoreState state = store->getState();
        state.active = Digest::of(test::TestFiles::bytes("update"));
        state.acceptedGeneration = 9;
        store->commit(state);
    }

    const std::unique_ptr<LocalStore> store = open(kAppBuild + 1);
    EXPECT_TRUE(store->getState().active.isZero());
    EXPECT_EQ(store->getState().acceptedGeneration, 9U);
    EXPECT_EQ(store->getState().appBuild, kAppBuild + 1);
}

TEST_F(LocalStoreTest, OpensEmptyForAnotherApp) {
    {
        const std::unique_ptr<LocalStore> store = open();
        StoreState state = store->getState();
        state.acceptedGeneration = 9;
        store->commit(state);
    }
    const LocalStore store(std::make_unique<FolderStoreFiles>(folder.getPath()), Manifest::identifyApplication("com.haylen.other"), kAppBuild);
    EXPECT_EQ(store.getState().acceptedGeneration, 0U);
}

TEST_F(LocalStoreTest, CollectsDeadRecordsAndMovesTheLiveRecordsOfMostlyDeadSegments) {
    std::vector<std::vector<std::uint8_t>> records;
    for (std::uint64_t seed = 0; seed < 12; ++seed) {
        records.push_back(makeRecord(seed));
    }
    const std::unique_ptr<LocalStore> store = open(kAppBuild, 4 * records[0].size() + 16);
    for (const std::vector<std::uint8_t>& record : records) {
        store->addRecord(key.getId(), record);
    }
    EXPECT_EQ(countSegments(), 3U);
    const Digest manifest = Digest::of(test::TestFiles::bytes("kept"));
    store->writeManifest(manifest, test::TestFiles::bytes("manifest"));
    store->writeManifest(Digest::of(test::TestFiles::bytes("dropped")), test::TestFiles::bytes("manifest"));

    // The first segment keeps all of its records, the second one of four and the third none.
    std::unordered_set<Digest, Digest::Hash> live;
    for (const std::size_t index : {0U, 1U, 2U, 3U, 5U}) {
        live.insert(idOf(records[index]));
    }
    const LocalStore::Collection collection = store->collect(live, {manifest}, {}, std::atomic<bool>(false));

    EXPECT_EQ(collection.removedRecords, 7U);
    EXPECT_EQ(collection.movedRecords, 1U);
    EXPECT_EQ(countSegments(), 2U);
    EXPECT_TRUE(store->readManifest(manifest).has_value());
    EXPECT_FALSE(store->readManifest(Digest::of(test::TestFiles::bytes("dropped"))).has_value());

    const std::unique_ptr<LocalStore> reopened = open();
    std::vector<std::uint8_t> read;
    Digest keyId;
    for (std::size_t index = 0; index < records.size(); ++index) {
        EXPECT_EQ(reopened->readRecord(idOf(records[index]), read, keyId), live.contains(idOf(records[index])));
        if (live.contains(idOf(records[index]))) {
            EXPECT_EQ(read, records[index]);
        }
    }
    EXPECT_FALSE(std::filesystem::exists(folder.getPath() / "journal-0"));
}

TEST_F(LocalStoreTest, MovesNoRecordsWithoutRoomForThem) {
    std::unique_ptr<LocalStore> store;
    const std::vector<std::vector<std::uint8_t>> records = fillSegments(store, LocalStore::kSafetyMargin);
    const std::unordered_set<Digest, Digest::Hash> live = keepSomeRecords(records);
    const LocalStore::Collection collection = store->collect(live, {}, {}, std::atomic<bool>(false));

    // The segment without live records goes, while the mostly dead one stays, since moving its record needs room the device lacks.
    EXPECT_EQ(collection.movedRecords, 0U);
    EXPECT_EQ(countSegments(), 2U);
    std::vector<std::uint8_t> read;
    Digest keyId;
    for (const std::vector<std::uint8_t>& record : records) {
        EXPECT_EQ(store->readRecord(idOf(record), read, keyId), live.contains(idOf(record)));
    }
}

TEST_F(LocalStoreTest, StopsACollectionAndLeavesTheStoreAsItWas) {
    std::unique_ptr<LocalStore> store;
    const std::vector<std::vector<std::uint8_t>> records = fillSegments(store, std::uint64_t{1} << 40);
    const StoreState before = store->getState();
    try {
        (void)store->collect(keepSomeRecords(records), {}, {}, std::atomic<bool>(true));
        FAIL() << "A stopped collection never switches to a new state.";
    } catch (const Error& error) {
        EXPECT_EQ(error.getCode(), Error::Code::UpdateInterrupted);
    }

    EXPECT_EQ(store->getState().journal, before.journal);
    const std::unique_ptr<LocalStore> reopened = open();
    std::vector<std::uint8_t> read;
    Digest keyId;
    for (const std::vector<std::uint8_t>& record : records) {
        ASSERT_TRUE(reopened->readRecord(idOf(record), read, keyId));
        EXPECT_EQ(read, record);
    }
}

// A collection that loses power while it moves a record, or while it writes the state that switches to its journal, leaves every record of the store where the earlier state finds it.
TEST_F(LocalStoreTest, KeepsEveryRecordWhenACollectionLosesPower) {
    for (const bool duringMove : {true, false}) {
        std::filesystem::remove_all(folder.getPath());
        std::filesystem::create_directories(folder.getPath());
        std::vector<std::vector<std::uint8_t>> records;
        for (std::uint64_t seed = 0; seed < 12; ++seed) {
            records.push_back(makeRecord(seed));
        }
        auto files = std::make_unique<test::CrashingStoreFiles>(folder.getPath());
        test::CrashingStoreFiles& crashing = *files;
        LocalStore store(std::move(files), application, kAppBuild, 4 * records[0].size() + 16);
        for (const std::vector<std::uint8_t>& record : records) {
            store.addRecord(key.getId(), record);
        }
        store.commit(store.getState());
        const StoreState before = store.getState();
        if (duringMove) {
            crashing.crashOnAppend(0);
        } else {
            crashing.crashOnWrite("state-");
        }
        EXPECT_THROW((void)store.collect(keepSomeRecords(records), {}, {}, std::atomic<bool>(false)), Error);
        EXPECT_TRUE(crashing.hasCrashed());

        const std::unique_ptr<LocalStore> reopened = open();
        EXPECT_EQ(reopened->getState().journal, before.journal);
        std::vector<std::uint8_t> read;
        Digest keyId;
        for (const std::vector<std::uint8_t>& record : records) {
            ASSERT_TRUE(reopened->readRecord(idOf(record), read, keyId)) << (duringMove ? "move" : "state");
            EXPECT_EQ(read, record);
        }
    }
}

TEST_F(LocalStoreTest, KeepsTheStartOfARecordWhoseDownloadStopped) {
    const std::unique_ptr<LocalStore> store = open();
    const Digest shard = Digest::of(test::TestFiles::bytes("shard"));
    const PartialRecord partial{.shardId = shard, .recordOffset = 160, .recordSize = 5000, .validator = "\"v1\"", .bytes = test::TestFiles::randomBytes(1000, 4)};
    store->writePartial(partial);

    const std::optional<PartialRecord> read = store->readPartial(shard, 160);
    ASSERT_TRUE(read);
    EXPECT_EQ(read->bytes, partial.bytes);
    EXPECT_EQ(read->validator, "\"v1\"");
    EXPECT_FALSE(store->readPartial(shard, 161));

    std::filesystem::resize_file(folder.getPath() / "staging" / (shard.toHex() + "-160.hpart"), 30);
    EXPECT_FALSE(store->readPartial(shard, 160));
    store->removePartial(shard, 160);
    EXPECT_FALSE(std::filesystem::exists(folder.getPath() / "staging" / (shard.toHex() + "-160.hpart")));
}

TEST(StoreFormatTest, ReadsWhatItWritesAndRefusesTornCopies) {
    const StoreState state{.application = Digest::of(test::TestFiles::bytes("app")), .appBuild = 5, .pending = Digest::of(test::TestFiles::bytes("update")), .channel = "beta", .acceptedGeneration = 7, .journal = 2, .sequence = 11};
    std::vector<std::uint8_t> bytes = state.write();
    EXPECT_EQ(StoreState::parse(bytes), state);
    bytes[40] ^= 1U;
    try {
        (void)StoreState::parse(bytes);
        FAIL() << "A torn state parses.";
    } catch (const Error& error) {
        EXPECT_STREQ(error.what(), "The store state has a checksum that does not match, so a write tore it.");
    }
    EXPECT_THROW((void)StoreState::parse(std::span(bytes).first(10)), Error);

    const StoreEntry entry{.storedId = Digest::of(test::TestFiles::bytes("record")), .segment = 3, .offset = 16, .size = 500};
    StoreEntry::Bytes written = entry.write();
    ASSERT_TRUE(StoreEntry::parse(written));
    EXPECT_EQ(StoreEntry::parse(written)->offset, 16U);
    written[70] ^= 1U;
    EXPECT_FALSE(StoreEntry::parse(written));
}

} // namespace haylen::content
