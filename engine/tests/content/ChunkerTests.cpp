#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <set>
#include <span>
#include <vector>

#include "content/crypto/Digest.hpp"
#include "content/format/Chunker.hpp"
#include "support/TestFiles.hpp"

namespace haylen::content {

class ChunkerTest : public ::testing::Test {
  protected:
    static constexpr std::size_t kMegabyte = 1024 * 1024;

    // Splits a whole file the way a content build does, window by window.
    static std::vector<std::size_t> split(std::span<const std::uint8_t> file) {
        std::vector<std::size_t> sizes;
        if (Chunker::isSingleChunk(file.size())) {
            sizes.push_back(file.size());
            return sizes;
        }
        for (std::size_t offset = 0; offset < file.size();) {
            const std::size_t cut = Chunker::findCut(file.subspan(offset, std::min(Chunker::kMaximumSize, file.size() - offset)));
            sizes.push_back(cut);
            offset += cut;
        }
        return sizes;
    }

    static std::set<Digest> identify(std::span<const std::uint8_t> file) {
        std::set<Digest> chunks;
        std::size_t offset = 0;
        for (const std::size_t size : split(file)) {
            chunks.insert(Digest::of(file.subspan(offset, size)));
            offset += size;
        }
        return chunks;
    }

    static std::size_t countShared(const std::set<Digest>& first, const std::set<Digest>& second) {
        return static_cast<std::size_t>(std::ranges::count_if(first, [&second](const Digest& chunk) { return second.contains(chunk); }));
    }
};

TEST_F(ChunkerTest, PinsTheCutsOfVersion1) {
    // These sizes pin `fastcdc-v1`, its gear table, masks and limits, as an independent implementation of the algorithm computes them. Content built with version 1 depends on them, so they never change.
    const std::vector<std::uint8_t> file = test::TestFiles::randomBytes(24 * kMegabyte, 1);
    EXPECT_EQ(split(file), (std::vector<std::size_t>{1127287, 1368008, 1414492, 1091171, 1060945, 1153255, 913309, 1373890, 1525918, 1125970, 1587257, 1299867, 1353906, 1164969, 2117420, 1399452, 1231432, 1388701, 1092609, 375966}));
}

TEST_F(ChunkerTest, KeepsChunksBetweenTheirLimits) {
    for (std::uint64_t seed = 2; seed <= 4; ++seed) {
        const std::vector<std::uint8_t> file = test::TestFiles::randomBytes(20 * kMegabyte + seed * 12345, seed);
        const std::vector<std::size_t> sizes = split(file);
        ASSERT_GT(sizes.size(), 4U);
        for (std::size_t index = 0; index + 1 < sizes.size(); ++index) {
            EXPECT_GE(sizes[index], Chunker::kMinimumSize);
            EXPECT_LE(sizes[index], Chunker::kMaximumSize);
        }
        EXPECT_LE(sizes.back(), Chunker::kMaximumSize);
        std::size_t total = 0;
        for (const std::size_t size : sizes) {
            total += size;
        }
        EXPECT_EQ(total, file.size());
    }

    // Data without content to cut at, such as zeros, cuts at the maximum size.
    const std::vector<std::uint8_t> zeros(9 * kMegabyte);
    EXPECT_EQ(split(zeros), (std::vector<std::size_t>{Chunker::kMaximumSize, Chunker::kMaximumSize, kMegabyte}));
}

TEST_F(ChunkerTest, KeepsSmallFilesWhole) {
    EXPECT_TRUE(Chunker::isSingleChunk(0));
    EXPECT_TRUE(Chunker::isSingleChunk(Chunker::kTargetSize));
    EXPECT_FALSE(Chunker::isSingleChunk(Chunker::kTargetSize + 1));
    const std::vector<std::uint8_t> file = test::TestFiles::randomBytes(Chunker::kTargetSize, 5);
    EXPECT_EQ(split(file), (std::vector<std::size_t>{Chunker::kTargetSize}));
    EXPECT_EQ(Chunker::findCut(std::span(file).first(1000)), 1000U);
}

TEST_F(ChunkerTest, KeepsLaterChunksAfterAnInsertionOrADeletion) {
    const std::vector<std::uint8_t> original = test::TestFiles::randomBytes(32 * kMegabyte, 6);
    const std::set<Digest> before = identify(original);

    std::vector<std::uint8_t> inserted = original;
    const std::vector<std::uint8_t> insertion = test::TestFiles::randomBytes(4321, 7);
    inserted.insert(inserted.begin() + 100000, insertion.begin(), insertion.end());
    EXPECT_GE(countShared(before, identify(inserted)), before.size() - 2) << "An insertion near the start changes only the chunks around it.";

    std::vector<std::uint8_t> deleted = original;
    deleted.erase(deleted.begin() + 5000000, deleted.begin() + 5000100);
    EXPECT_GE(countShared(before, identify(deleted)), before.size() - 2);

    std::vector<std::uint8_t> edited = original;
    std::fill_n(edited.begin() + 20000000, 64, std::uint8_t{0});
    EXPECT_GE(countShared(before, identify(edited)), before.size() - 2);
}

} // namespace haylen::content
