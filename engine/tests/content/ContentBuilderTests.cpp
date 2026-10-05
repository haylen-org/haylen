#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "content/ContentBuilder.hpp"
#include "content/crypto/KeyRing.hpp"
#include "content/format/Catalog.hpp"
#include "content/format/Chunker.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "support/ReleaseFixture.hpp"
#include "support/TemporaryDirectory.hpp"
#include "support/TestFiles.hpp"

namespace haylen::content {

class ContentBuilderTest : public ::testing::Test {
  protected:
    using Files = std::map<std::string, std::vector<std::uint8_t>>;

    static constexpr std::size_t kMegabyte = 1024 * 1024;

    ContentBuilderTest() {
        keyId = keys.add(test::ReleaseFixture::makeKey(1));
    }

    // A release of a large file, which splits into many chunks, and of many small files, which stay one chunk each.
    [[nodiscard]] static Files makeFiles() {
        Files files{{"content/world/terrain.bin", test::TestFiles::randomBytes(12 * kMegabyte, 1)}};
        for (int index = 0; index < 20; ++index) {
            files.emplace("content/data/item" + std::to_string(index) + ".json", test::TestFiles::bytes("{\"item\": " + std::to_string(index) + ", \"name\": \"Item number " + std::to_string(index) + "\"}"));
        }
        return files;
    }

    [[nodiscard]] ContentBuilder::Result build(const std::filesystem::path& folder, const Files& files, const ContentBuilder::Result* previous = nullptr, std::uint64_t target = ContentBuilder::kDefaultShardTarget) const {
        ContentBuilder builder(folder, keys.get(keyId), target);
        if (previous != nullptr) {
            const Catalog catalog = Catalog::parse(previous->catalog, previous->shards.size());
            builder.reuse(catalog, previous->shards);
        }
        std::vector<ContentBuilder::Input> inputs;
        for (const auto& [path, bytes] : files) {
            inputs.push_back({.path = path});
        }
        return builder.build(io::MemoryPackage("source", files), std::move(inputs));
    }

    [[nodiscard]] static std::vector<std::uint8_t> readFile(const std::filesystem::path& file) {
        std::ifstream stream(file, std::ios::binary);
        return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    }

    KeyRing keys;
    Digest keyId;
    test::TemporaryDirectory directory;
};

TEST_F(ContentBuilderTest, BuildsTheSameBytesInEveryCleanBuild) {
    const Files files = makeFiles();
    const ContentBuilder::Result first = build(directory.getPath() / "first", files);
    const ContentBuilder::Result second = build(directory.getPath() / "second", files);
    EXPECT_EQ(first.catalog, second.catalog);
    ASSERT_EQ(first.shards, second.shards);
    for (const ShardReference& shard : first.shards) {
        EXPECT_EQ(readFile(directory.getPath() / "first" / shard.getFileName()), readFile(directory.getPath() / "second" / shard.getFileName()));
    }
    EXPECT_GE(first.statistics.newChunks, 21U + 12 * kMegabyte / Chunker::kMaximumSize);
}

TEST_F(ContentBuilderTest, IgnoresTheOrderOfItsInputs) {
    const Files files = makeFiles();
    const ContentBuilder::Result ordered = build(directory.getPath() / "ordered", files);

    ContentBuilder builder(directory.getPath() / "reversed", keys.get(keyId));
    std::vector<ContentBuilder::Input> inputs;
    for (const auto& [path, bytes] : files) {
        inputs.insert(inputs.begin(), {.path = path});
    }
    const ContentBuilder::Result reversed = builder.build(io::MemoryPackage("source", files), std::move(inputs));
    EXPECT_EQ(reversed.catalog, ordered.catalog);
    EXPECT_EQ(reversed.shards, ordered.shards);
    EXPECT_THROW((void)ContentBuilder(directory.getPath() / "twice", keys.get(keyId)).build(io::MemoryPackage("source", files), {{.path = "content/data/item1.json"}, {.path = "content/data/item1.json"}}), std::invalid_argument);
}

TEST_F(ContentBuilderTest, StoresOnlyTheChunksAnUpdateChanged) {
    Files files = makeFiles();
    const ContentBuilder::Result base = build(directory.getPath(), files);
    std::map<std::string, std::vector<std::uint8_t>> before;
    for (const ShardReference& shard : base.shards) {
        before.emplace(shard.getFileName(), readFile(directory.getPath() / shard.getFileName()));
    }

    // One small region of the large file and one small file change.
    std::fill_n(files["content/world/terrain.bin"].begin() + 7 * kMegabyte, 64, std::uint8_t{7});
    files["content/data/item3.json"] = test::TestFiles::bytes("{\"item\": 3, \"name\": \"Renamed\"}");
    const ContentBuilder::Result patch = build(directory.getPath(), files, &base);

    EXPECT_LE(patch.statistics.newChunks, 3U);
    EXPECT_LE(patch.statistics.newBytes, 2 * Chunker::kMaximumSize + 100);
    EXPECT_GE(patch.statistics.reusedChunks, base.statistics.newChunks - 3);
    ASSERT_EQ(patch.shards.size(), base.shards.size() + 1);
    EXPECT_TRUE(std::equal(base.shards.begin(), base.shards.end(), patch.shards.begin())) << "The update references the old shards as they are.";
    for (const auto& [name, bytes] : before) {
        EXPECT_EQ(readFile(directory.getPath() / name), bytes) << "An update never rewrites an old shard.";
    }
    EXPECT_LT(patch.shards.back().fileSize, 2 * Chunker::kMaximumSize + 4096) << "The patch shard holds only the changed chunks.";
}

TEST_F(ContentBuilderTest, KeepsLaterChunksAfterAnInsertion) {
    Files files = makeFiles();
    const ContentBuilder::Result base = build(directory.getPath(), files);

    std::vector<std::uint8_t>& terrain = files["content/world/terrain.bin"];
    const std::vector<std::uint8_t> insertion = test::TestFiles::randomBytes(1000, 99);
    terrain.insert(terrain.begin() + 50000, insertion.begin(), insertion.end());
    const ContentBuilder::Result patch = build(directory.getPath(), files, &base);
    EXPECT_LE(patch.statistics.newChunks, 2U) << "Content-defined chunks find their old boundaries again after an insertion near the start.";
}

TEST_F(ContentBuilderTest, ReusesRenamedAndRepeatedFiles) {
    Files files = makeFiles();
    const ContentBuilder::Result base = build(directory.getPath(), files);

    files["content/world/renamed.bin"] = files["content/world/terrain.bin"];
    files.erase("content/world/terrain.bin");
    files["content/world/copy.bin"] = files["content/world/renamed.bin"];
    const ContentBuilder::Result patch = build(directory.getPath(), files, &base);
    EXPECT_EQ(patch.statistics.newChunks, 0U);
    EXPECT_EQ(patch.shards, base.shards);
}

TEST_F(ContentBuilderTest, DropsShardsThatNoFileUsesAnymore) {
    const ContentBuilder::Result base = build(directory.getPath(), {{"content/a.bin", test::TestFiles::randomBytes(1000, 1)}});
    const ContentBuilder::Result patch = build(directory.getPath(), {{"content/b.bin", test::TestFiles::randomBytes(1000, 2)}}, &base);
    ASSERT_EQ(patch.shards.size(), 1U);
    EXPECT_NE(patch.shards.front(), base.shards.front());
}

TEST_F(ContentBuilderTest, ClosesShardsNearTheirTarget) {
    // A small target makes several shards, and files small against the target never split between two of them.
    const std::uint64_t target = 3 * kMegabyte;
    Files files = makeFiles();
    files.emplace("content/world/rivers.bin", test::TestFiles::randomBytes(100000, 2));
    const ContentBuilder::Result result = build(directory.getPath(), files, nullptr, target);
    ASSERT_GE(result.shards.size(), 4U);
    for (const ShardReference& shard : result.shards) {
        EXPECT_LE(shard.fileSize, target + Chunker::kMaximumSize);
    }

    const Catalog catalog = Catalog::parse(result.catalog, result.shards.size());
    for (std::uint64_t index = 0; index < catalog.getFileCount(); ++index) {
        const Catalog::File file = catalog.getFile(index);
        if (file.size <= target / 16) {
            std::set<std::uint32_t> shards;
            for (std::uint64_t part = file.firstPart; part < file.firstPart + file.partCount; ++part) {
                shards.insert(catalog.getChunk(catalog.getPart(part).chunk).shard);
            }
            EXPECT_EQ(shards.size(), 1U) << file.path;
        }
    }
}

} // namespace haylen::content
