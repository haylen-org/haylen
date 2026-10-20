#include <gtest/gtest.h>
#include <lua.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "content/FolderStoreFiles.hpp"
#include "content/HpakPackage.hpp"
#include "content/LocalStore.hpp"
#include "content/ReleasePackage.hpp"
#include "content/ShardSet.hpp"
#include "content/ShardWriter.hpp"
#include "content/format/CatalogWriter.hpp"
#include "content/format/ChunkRecord.hpp"
#include "content/format/Chunker.hpp"
#include "content/format/RecordSealer.hpp"
#include "content/format/ShardHeader.hpp"
#include "haylen/content/Error.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "haylen/lua/Application.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/AllocationTracker.hpp"
#include "support/CountingPackage.hpp"
#include "support/ReleaseFixture.hpp"
#include "support/TemporaryDirectory.hpp"
#include "support/TestFiles.hpp"

namespace haylen::content {

class HpakPackageTest : public ::testing::Test {
  protected:
    static constexpr std::size_t kMegabyte = 1024 * 1024;

    [[nodiscard]] static std::map<std::string, std::vector<std::uint8_t>> makeFiles() {
        std::map<std::string, std::vector<std::uint8_t>> files;
        const auto text = [&files](const std::string& path, const std::string& content) { files.emplace(path, test::TestFiles::bytes(content)); };
        text("app.json", R"({"name": "Release Test", "identifier": "com.haylen.tests", "plugins": {"ads": {}}})");
        text("source/main.lua", "local menu = require('scenes.menu')\nlocal assets = require('haylen.assets')\nloaded = menu.name .. ' ' .. assets.json('data/config.json').level .. ' ' .. assets.fileSize('world/terrain.bin')");
        text("source/scenes/menu.lua", "return {name = 'menu'}");
        text("plugins/ads/plugin.json", R"({"id": "ads", "version": "1.0.0"})");
        text("plugins/ads/source/init.lua", "return {}");
        text("content/data/config.json", R"({"level": 3})");
        text("content/empty.txt", "");
        files.emplace("content/world/terrain.bin", test::TestFiles::randomBytes(9 * kMegabyte, 1));
        return files;
    }

    void build(const std::map<std::string, std::vector<std::uint8_t>>& files) {
        release.build(io::MemoryPackage("source", files));
    }

    [[nodiscard]] std::shared_ptr<io::Package> open() const {
        return release.open();
    }

    static void expectCode(Error::Code code, const std::function<void()>& action) {
        try {
            action();
            ADD_FAILURE() << "The action must fail.";
        } catch (const Error& error) {
            EXPECT_EQ(error.getCode(), code) << error.what();
        }
    }

    // Changes one byte of a file of the release folder.
    void damage(const std::filesystem::path& file, std::uint64_t offset) const {
        std::fstream stream(release.getFolder() / file, std::ios::binary | std::ios::in | std::ios::out);
        stream.seekg(static_cast<std::streamoff>(offset));
        const auto byte = static_cast<char>(stream.get() ^ 0x01);
        stream.seekp(static_cast<std::streamoff>(offset));
        stream.put(byte);
    }

    [[nodiscard]] std::vector<std::string> listShards() const {
        std::vector<std::string> shards;
        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(release.getFolder())) {
            if (entry.path().extension() == ".hpak") {
                shards.push_back(entry.path().filename().string());
            }
        }
        std::ranges::sort(shards);
        return shards;
    }

    test::ReleaseFixture release;
};

TEST_F(HpakPackageTest, ServesTheFilesOfTheApp) {
    const auto files = makeFiles();
    build(files);
    const std::shared_ptr<io::Package> package = open();

    // The Lua modules of the app and its plugins ship as bytecode, and every other file as it is.
    std::vector<std::string> paths;
    for (const auto& [path, bytes] : files) {
        paths.push_back(path);
        EXPECT_TRUE(package->exists(path));
        const bool module = path.ends_with(".lua");
        EXPECT_EQ(package->isLuaBytecode(path), module) << path;
        if (module) {
            const std::vector<std::uint8_t> chunk = package->read(path);
            EXPECT_EQ(std::string(chunk.begin(), chunk.begin() + 4), LUA_SIGNATURE) << path;
            continue;
        }
        EXPECT_EQ(package->getFileSize(path), bytes.size());
        EXPECT_EQ(package->read(path), bytes) << path;
    }
    EXPECT_EQ(package->list(""), paths);
    EXPECT_EQ(package->list("source"), (std::vector<std::string>{"source/main.lua", "source/scenes/menu.lua"}));
    EXPECT_EQ(package->listAssets("world"), (std::vector<std::string>{"world/terrain.bin"}));
    EXPECT_EQ(package->readAssetText("data/config.json"), R"({"level": 3})");
    EXPECT_TRUE(package->read("content/empty.txt").empty());
    EXPECT_FALSE(package->exists("content/missing.png"));
    EXPECT_THROW((void)package->read("content/missing.png"), std::runtime_error);
    EXPECT_THROW((void)package->read("../outside"), std::invalid_argument);
    EXPECT_EQ(core::AppConfig::fromPackage(*package).name, "Release Test");

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(release.getFolder())) {
        const std::string name = entry.path().filename().string();
        EXPECT_TRUE(name.ends_with(".hpak") || name.ends_with(".hmanifest")) << "The release holds no plain file: " << name;
    }
}

TEST_F(HpakPackageTest, ReadsRangesAcrossChunks) {
    const auto files = makeFiles();
    build(files);
    const std::shared_ptr<io::Package> package = open();
    const std::vector<std::uint8_t>& terrain = files.at("content/world/terrain.bin");

    for (const std::uint64_t offset : {std::uint64_t{0}, std::uint64_t{1}, std::uint64_t{Chunker::kMinimumSize - 3}, std::uint64_t{3 * kMegabyte + 12345}, std::uint64_t{terrain.size() - 10}}) {
        const std::vector<std::uint8_t> range = package->readAssetRange("world/terrain.bin", offset, 2 * kMegabyte);
        const std::size_t expected = static_cast<std::size_t>(std::min<std::uint64_t>(2 * kMegabyte, terrain.size() - offset));
        ASSERT_EQ(range.size(), expected);
        EXPECT_TRUE(std::equal(range.begin(), range.end(), terrain.begin() + static_cast<std::ptrdiff_t>(offset))) << offset;
    }
    EXPECT_TRUE(package->readAssetRange("world/terrain.bin", terrain.size(), 10).empty());

    // Reading the file in pieces that never line up with its chunks gives back every byte.
    const std::unique_ptr<io::PackageReader> reader = package->openReader("content/world/terrain.bin");
    std::vector<std::uint8_t> streamed(terrain.size());
    for (std::size_t offset = 0; offset < streamed.size(); offset += 777777) {
        reader->readExactly(offset, std::span(streamed).subspan(offset, std::min<std::size_t>(777777, streamed.size() - offset)));
    }
    EXPECT_EQ(streamed, terrain);
    std::array<std::uint8_t, 4> beyond{};
    EXPECT_EQ(reader->read(terrain.size(), beyond), 0U);
}

TEST_F(HpakPackageTest, ReadsOnlyTheChunksOfARange) {
    build(makeFiles());
    const auto files = std::make_shared<test::CountingPackage>(release.getFiles());
    const std::shared_ptr<io::Package> package = release.open(files);
    std::uint64_t shardBytes = 0;
    for (const std::string& shard : listShards()) {
        shardBytes += std::filesystem::file_size(release.getFolder() / shard);
    }

    // A small asset reads the header and the index of its shard and its one record, never the rest of the shard.
    files->reset();
    EXPECT_EQ(package->readAssetText("data/config.json"), R"({"level": 3})");
    EXPECT_LT(files->getReadBytes(), 64 * 1024U);

    files->reset();
    std::vector<std::uint8_t> range;
    {
        const test::AllocationTracker tracker;
        range = package->readAssetRange("world/terrain.bin", 5 * kMegabyte, 100);
        EXPECT_LT(tracker.getLargest(), Chunker::kMaximumSize + 4096) << "No allocation holds more than one chunk.";
    }
    EXPECT_EQ(range.size(), 100U);
    EXPECT_LE(files->getReadBytes(), Chunker::kMaximumSize + ChunkRecord::kHeaderSize + ShardHeader::kSize + 64 * 1024);
    EXPECT_LE(files->getLargestRead(), Chunker::kMaximumSize + ChunkRecord::kHeaderSize);
    EXPECT_GT(shardBytes, 9 * kMegabyte);
}

TEST_F(HpakPackageTest, MountsHundredsOfGigabytesInBoundedMemory) {
    // One zero chunk of the largest size repeats to form a file of 300 GiB, so the package describes far more than any device holds.
    test::TemporaryDirectory folder;
    const std::vector<std::uint8_t> zeros(Chunker::kMaximumSize);
    std::vector<std::uint8_t> ciphertext;
    const ChunkRecord record = RecordSealer::seal(*release.getContentKey(), zeros, ChunkRecord::identifyContent(zeros), ciphertext);
    ShardWriter writer(folder.getPath(), release.getContentKey());
    writer.add(record, ciphertext);
    const ShardReference reference = writer.finish();

    const std::uint64_t size = std::uint64_t{300} << 30;
    CatalogWriter catalogWriter;
    catalogWriter.addChunk({.storedId = record.storedId, .contentId = record.contentId, .plainSize = record.plainSize, .encodedSize = record.encodedSize, .codec = record.codec, .profile = record.profile});
    catalogWriter.addFile("content/world.bin", Delivery::Required, std::vector<Digest>(static_cast<std::size_t>(size / Chunker::kMaximumSize), record.storedId));
    std::vector<std::uint8_t> catalogBytes = catalogWriter.write();

    const std::shared_ptr<const io::Package> files = io::Package::openDirectory(folder.getPath());
    std::unique_ptr<HpakPackage> package;
    std::size_t mountedBytes = 0;
    {
        const test::AllocationTracker tracker;
        // clang-format off
        package = std::make_unique<HpakPackage>("huge", std::make_shared<const Catalog>(Catalog::parse(std::move(catalogBytes), 1)), std::make_shared<const ShardSet>(std::vector{reference}, release.getKeys(), [files](const ShardReference& shard) {
            return files->openReader(shard.getFileName());
        }));
        // clang-format on
        mountedBytes = tracker.getTotal();
    }
    EXPECT_LT(mountedBytes, std::size_t{1} << 20) << "Mounting allocates nothing in proportion to the size the package describes.";
    EXPECT_EQ(package->getFileSize("content/world.bin"), size);

    {
        const test::AllocationTracker tracker;
        const std::vector<std::uint8_t> range = package->readRange("content/world.bin", (std::uint64_t{250} << 30) + 12345, 100);
        EXPECT_EQ(range, std::vector<std::uint8_t>(100));
        EXPECT_LT(tracker.getTotal(), 3 * Chunker::kMaximumSize) << "A read decrypts one chunk, whatever the size of its file.";
    }
    try {
        (void)package->read("content/world.bin");
        FAIL() << "A whole read of a huge file must fail.";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find("Read it in ranges instead."), std::string::npos);
    }

    // A range read that asks for more than one read allows fails before it allocates anything for the range.
    {
        const test::AllocationTracker tracker;
        try {
            (void)package->readRange("content/world.bin", size - io::Package::kMaxReadSize - 1, std::numeric_limits<std::size_t>::max());
            ADD_FAILURE() << "A range read longer than the limit must fail.";
        } catch (const std::runtime_error& error) {
            EXPECT_NE(std::string(error.what()).find("Read it in smaller ranges instead."), std::string::npos);
        }
        EXPECT_LT(tracker.getLargest(), std::size_t{1} << 20);
    }
}

TEST_F(HpakPackageTest, RefusesChunksOfAnotherSizeThanTheCatalogNames) {
    // A catalog that gives a sealed chunk twice its plain size describes a file larger than its chunks hold, and a reader never reads past the chunk it decrypted.
    test::TemporaryDirectory folder;
    const std::vector<std::uint8_t> plain = test::TestFiles::randomBytes(1000, 3);
    std::vector<std::uint8_t> ciphertext;
    const ChunkRecord record = RecordSealer::seal(*release.getContentKey(), plain, ChunkRecord::identifyContent(plain), ciphertext);
    ShardWriter writer(folder.getPath(), release.getContentKey());
    writer.add(record, ciphertext);
    const ShardReference reference = writer.finish();

    CatalogWriter catalogWriter;
    catalogWriter.addChunk({.storedId = record.storedId, .contentId = record.contentId, .plainSize = 2 * record.plainSize, .encodedSize = 2 * record.encodedSize, .codec = record.codec, .profile = record.profile});
    catalogWriter.addFile("content/data.bin", Delivery::Required, std::vector<Digest>{record.storedId});
    const std::shared_ptr<const io::Package> files = io::Package::openDirectory(folder.getPath());
    // clang-format off
    const HpakPackage package("forged", std::make_shared<const Catalog>(Catalog::parse(catalogWriter.write(), 1)), std::make_shared<const ShardSet>(std::vector{reference}, release.getKeys(), [files](const ShardReference& shard) {
        return files->openReader(shard.getFileName());
    }));
    // clang-format on

    EXPECT_EQ(package.getFileSize("content/data.bin"), 2 * plain.size());
    expectCode(Error::Code::CorruptChunk, [&] { (void)package.readRange("content/data.bin", plain.size() + 500, 100); });
    expectCode(Error::Code::CorruptChunk, [&] { (void)package.readRange("content/data.bin", 0, 100); });
}

TEST_F(HpakPackageTest, NamesAMissingFileByItsPathInThePackage) {
    // clang-format off
    const HpakPackage package("app.hmanifest", std::make_shared<const Catalog>(Catalog::parse(CatalogWriter().write(), 0)), std::make_shared<const ShardSet>(std::vector<ShardReference>{}, release.getKeys(), [](const ShardReference&) {
        return std::unique_ptr<io::PackageReader>();
    }));
    // clang-format on
    const std::string message = "The package file \"app.hmanifest/content/missing.png\" was not found.";
    for (const std::function<void()>& action : std::vector<std::function<void()>>{[&] { (void)package.getFileSize("content/missing.png"); }, [&] { (void)package.openReader("./content/missing.png"); }, [&] { (void)package.isLuaBytecode("content/missing.png"); }}) {
        try {
            action();
            ADD_FAILURE() << "A missing file must fail.";
        } catch (const std::runtime_error& error) {
            EXPECT_EQ(std::string(error.what()), message);
        }
    }
}

TEST_F(HpakPackageTest, ReadsTheChunksOfAShardItLacksFromTheLocalStore) {
    test::TemporaryDirectory folder;
    const std::vector<std::uint8_t> first = test::TestFiles::randomBytes(5000, 7);
    const std::vector<std::uint8_t> second = test::TestFiles::randomBytes(3000, 8);
    CatalogWriter catalogWriter;
    std::vector<Digest> parts;
    std::vector<std::vector<std::uint8_t>> records;
    for (const std::vector<std::uint8_t>& plain : {first, second}) {
        std::vector<std::uint8_t> ciphertext;
        const ChunkRecord record = RecordSealer::seal(*release.getContentKey(), plain, ChunkRecord::identifyContent(plain), ciphertext);
        catalogWriter.addChunk({.storedId = record.storedId, .contentId = record.contentId, .plainSize = record.plainSize, .encodedSize = record.encodedSize, .codec = record.codec, .profile = record.profile});
        parts.push_back(record.storedId);
        const ChunkRecord::Header header = record.serialize();
        records.emplace_back(header.begin(), header.end()).insert(records.back().end(), ciphertext.begin(), ciphertext.end());
    }
    catalogWriter.addFile("content/level.bin", Delivery::OnDemand, parts);
    const auto catalog = std::make_shared<const Catalog>(Catalog::parse(catalogWriter.write(), 1));
    const ShardReference reference{.shardId = Digest::of(test::TestFiles::bytes("remote shard")), .fileSize = 1000};
    const auto store = std::make_shared<LocalStore>(std::make_unique<FolderStoreFiles>(folder.getPath() / "store"), Digest(), 1);
    const auto missing = [](const ShardReference&) { return std::unique_ptr<io::PackageReader>(); };
    const HpakPackage package("store", catalog, std::make_shared<const ShardSet>(std::vector{reference}, release.getKeys(), missing, store));
    const HpakPackage bundled("bundled", catalog, std::make_shared<const ShardSet>(std::vector{reference}, release.getKeys(), missing));

    const auto expectCode = [](Error::Code code, const std::function<void()>& action) {
        try {
            action();
            FAIL() << "The read must fail.";
        } catch (const Error& error) {
            EXPECT_EQ(error.getCode(), code) << error.what();
        }
    };
    expectCode(Error::Code::MissingShard, [&] { (void)bundled.read("content/level.bin"); });
    store->addRecord(release.getContentKey()->getId(), records[0]);
    expectCode(Error::Code::MissingChunk, [&] { (void)package.read("content/level.bin"); });

    store->addRecord(release.getContentKey()->getId(), records[1]);
    std::vector<std::uint8_t> whole = first;
    whole.insert(whole.end(), second.begin(), second.end());
    EXPECT_EQ(package.read("content/level.bin"), whole);

    // A record that the device damaged leaves the store, and a download brings it back.
    store->flush();
    for (const std::filesystem::directory_entry& segment : std::filesystem::directory_iterator(folder.getPath() / "store" / "segments")) {
        std::fstream stream(segment.path(), std::ios::binary | std::ios::in | std::ios::out);
        stream.seekp(-10, std::ios::end);
        stream.put('x');
    }
    expectCode(Error::Code::CacheCorrupt, [&] { (void)package.readRange("content/level.bin", 5000, 10); });
    EXPECT_FALSE(store->contains(parts[1]));
    EXPECT_TRUE(store->contains(parts[0]));
}

TEST_F(HpakPackageTest, ReadsFromManyThreadsAtOnce) {
    const auto files = makeFiles();
    build(files);
    const std::shared_ptr<io::Package> package = open();
    const std::vector<std::uint8_t>& terrain = files.at("content/world/terrain.bin");
    const std::unique_ptr<io::PackageReader> shared = package->openReader("content/world/terrain.bin");

    std::vector<std::thread> threads;
    std::vector<int> matches(8, 0);
    for (std::size_t thread = 0; thread < matches.size(); ++thread) {
        // clang-format off
        threads.emplace_back([&, thread] {
            for (std::size_t round = 0; round < 6; ++round) {
                const std::uint64_t offset = (thread * 1234567 + round * 765432) % (terrain.size() - 5000);
                std::vector<std::uint8_t> own = package->readRange("content/world/terrain.bin", offset, 5000);
                std::vector<std::uint8_t> through(5000);
                shared->readExactly(offset, through);
                const bool same = std::equal(own.begin(), own.end(), terrain.begin() + static_cast<std::ptrdiff_t>(offset)) && own == through;
                matches[thread] += same ? 1 : 0;
            }
        });
        // clang-format on
    }
    for (std::thread& thread : threads) {
        thread.join();
    }
    EXPECT_EQ(matches, std::vector<int>(8, 6));
}

// A release of more shards than stay open serves every file from several threads at once through a bounded number of open shard files, and a shard that closed opens again when a file needs it.
TEST_F(HpakPackageTest, KeepsABoundedNumberOfShardsOpen) {
    std::map<std::string, std::vector<std::uint8_t>> files;
    files.emplace("app.json", test::TestFiles::bytes(R"({"name": "Release Test", "identifier": "com.haylen.tests"})"));
    files.emplace("source/main.lua", test::TestFiles::bytes("return {}"));
    std::vector<std::string> parts;
    for (std::uint32_t part = 0; part < ShardSet::kMaxOpenShards + 8; ++part) {
        parts.push_back("parts/part_" + std::to_string(100 + part) + ".bin");
        files.emplace("content/" + parts.back(), test::TestFiles::randomBytes(40 * 1024, part + 1));
    }
    release.build(io::MemoryPackage("source", files), 64 * 1024);
    ASSERT_GE(listShards().size(), parts.size());
    const auto counting = std::make_shared<test::CountingPackage>(release.getFiles());
    const std::shared_ptr<io::Package> package = release.open(counting);

    std::vector<std::thread> threads;
    std::vector<std::size_t> matches(4, 0);
    for (std::size_t thread = 0; thread < matches.size(); ++thread) {
        // clang-format off
        threads.emplace_back([&, thread] {
            for (std::size_t index = 0; index < 2 * parts.size(); ++index) {
                const std::string& part = parts[(index * (thread + 1) + thread * 5) % parts.size()];
                matches[thread] += package->readAsset(part) == files.at("content/" + part) ? 1U : 0U;
            }
        });
        // clang-format on
    }
    for (std::thread& thread : threads) {
        thread.join();
    }
    EXPECT_EQ(matches, std::vector<std::size_t>(4, 2 * parts.size()));
    EXPECT_LE(counting->getOpenReaders(), ShardSet::kMaxOpenShards + 1);
    EXPECT_LE(counting->getMostOpenReaders(), ShardSet::kMaxOpenShards + 1 + matches.size());
}

TEST_F(HpakPackageTest, ReportsMissingAndDamagedContent) {
    build(makeFiles());
    const std::vector<std::string> shards = listShards();
    ASSERT_EQ(shards.size(), 2U);

    // A damaged record fails only the reads that need it, and the middle of the largest shard belongs to the large file.
    const std::string largest = *std::ranges::max_element(shards, {}, [this](const std::string& shard) { return std::filesystem::file_size(release.getFolder() / shard); });
    damage(largest, std::filesystem::file_size(release.getFolder() / largest) / 2);
    {
        const std::shared_ptr<io::Package> damaged = open();
        EXPECT_EQ(damaged->readAssetText("data/config.json"), R"({"level": 3})");
        expectCode(Error::Code::ChunkAuthenticationFailed, [&] { (void)damaged->readAsset("world/terrain.bin"); });
    }

    // A missing shard keeps the package listing its files and fails the reads of them. The damaged package closed its shards first, since Windows deletes no file that is open.
    for (const std::string& shard : shards) {
        std::filesystem::remove(release.getFolder() / shard);
    }
    const std::shared_ptr<io::Package> missing = open();
    EXPECT_TRUE(missing->exists("source/main.lua"));
    expectCode(Error::Code::MissingShard, [&] { (void)missing->read("source/main.lua"); });
}

TEST_F(HpakPackageTest, RejectsReleasesThatAreNotForThisApp) {
    build(makeFiles());
    damage("app.hmanifest", 100);
    expectCode(Error::Code::ManifestSignatureInvalid, [&] { (void)open(); });
    damage("app.hmanifest", 100);
    (void)open();

    content::Compatibility newer = release.getCompatibility();
    newer.appBuild = test::ReleaseFixture::kAppBuild + 1;
    const VerifyingKey trusted = release.getSigningKey().getVerifyingKey();
    expectCode(Error::Code::ManifestIncompatible, [&] { (void)ReleasePackage::open(release.getFiles(), release.getKeys(), std::span(&trusted, 1), newer); });

    std::filesystem::copy_file(release.getFolder() / "app.hmanifest", release.getFolder() / "content.hmanifest", std::filesystem::copy_options::overwrite_existing);
    expectCode(Error::Code::CorruptManifest, [&] { (void)open(); });
}

TEST_F(HpakPackageTest, AppliesUpdatesOverTheBase) {
    auto files = makeFiles();
    build(files);
    const std::vector<std::string> base = listShards();

    files["content/data/config.json"] = test::TestFiles::bytes(R"({"level": 4})");
    files["content/data/new.json"] = test::TestFiles::bytes(R"({"new": true})");
    build(files);
    const std::vector<std::string> updated = listShards();
    EXPECT_EQ(updated.size(), base.size() + 1) << "The update adds one patch shard next to the base shards.";
    EXPECT_EQ(release.getStatistics(Manifest::Domain::App).newChunks, 0U);
    EXPECT_EQ(release.getStatistics(Manifest::Domain::Content).newChunks, 2U);

    const std::shared_ptr<io::Package> package = open();
    EXPECT_EQ(package->readAssetText("data/config.json"), R"({"level": 4})");
    EXPECT_EQ(package->readAssetText("data/new.json"), R"({"new": true})");
    EXPECT_EQ(package->readAsset("world/terrain.bin"), files.at("content/world/terrain.bin"));
}

TEST_F(HpakPackageTest, RunsAnAppFromItsRelease) {
    build(makeFiles());
    const std::shared_ptr<io::Package> package = open();
    test::TemporaryDirectory data;
    platform::HeadlessHost host(data.getPath());
    core::AppConfig config = core::AppConfig::fromPackage(*package);
    auto application = std::make_unique<lua::Application>();
    application->configure(config);
    core::Engine engine(host, package, std::move(config), std::move(application));
    engine.start();
    engine.frame(1.0 / 60.0);

    lua_State* L = engine.getLuaState();
    lua_getglobal(L, "loaded");
    EXPECT_STREQ(lua_tostring(L, -1), "menu 3 9437184");
    lua_pop(L, 1);
}

} // namespace haylen::content
