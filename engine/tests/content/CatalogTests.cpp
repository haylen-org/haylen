#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "content/Error.hpp"
#include "content/format/Catalog.hpp"
#include "content/format/CatalogWriter.hpp"
#include "support/TestFiles.hpp"

namespace haylen::content {

class ContentCatalogTest : public ::testing::Test {
  protected:
    [[nodiscard]] static Catalog::Chunk makeChunk(std::uint8_t id, std::uint64_t size, std::uint32_t shard = 0) {
        const std::vector<std::uint8_t> seed{id};
        return {.storedId = Digest::of(seed), .contentId = Digest::of(test::TestFiles::bytes("content " + std::to_string(id))), .plainSize = size, .encodedSize = size, .shard = shard};
    }

    // A catalog of a few files over shared chunks, some of them in a second shard.
    [[nodiscard]] static std::vector<std::uint8_t> makeBytes() {
        CatalogWriter writer;
        const Catalog::Chunk first = makeChunk(1, 1000);
        const Catalog::Chunk second = makeChunk(2, 500, 1);
        const Catalog::Chunk third = makeChunk(3, 1);
        writer.addChunk(first);
        writer.addChunk(second);
        writer.addChunk(third);
        writer.addFile("content/maps/island.tmj", Delivery::Required, std::vector{first.storedId, second.storedId, first.storedId});
        writer.addFile("content/maps/cave.tmj", Delivery::OnDemand, std::vector{third.storedId});
        writer.addFile("content/maps-extra/a.png", Delivery::Prefetch, std::vector{second.storedId});
        writer.addFile("content/empty.txt", Delivery::Required, {});
        writer.addFile("content/maps/tiles/grass.png", Delivery::Required, std::vector{third.storedId});
        return writer.write();
    }

    static void expectCorrupt(std::vector<std::uint8_t> bytes, std::uint64_t shardCount = 2) {
        try {
            const Catalog catalog = Catalog::parse(std::move(bytes), shardCount);
            ADD_FAILURE() << "The catalog must be rejected.";
        } catch (const Error& error) {
            EXPECT_TRUE(error.getCode() == Error::Code::CorruptCatalog || error.getCode() == Error::Code::UnsupportedFormat || error.getCode() == Error::Code::UnsupportedVersion) << error.what();
        }
    }

    [[nodiscard]] static std::vector<std::string> listFolder(const Catalog& catalog, std::string_view folder) {
        std::vector<std::string> found;
        const auto [first, last] = catalog.findFolder(folder);
        for (std::uint64_t index = first; index < last; ++index) {
            found.emplace_back(catalog.getFile(index).path);
        }
        return found;
    }

    static void setU64(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint64_t value) {
        for (std::size_t index = 0; index < 8; ++index) {
            bytes[offset + index] = static_cast<std::uint8_t>(value >> (8 * index));
        }
    }
};

TEST_F(ContentCatalogTest, ReadsBackWhatItWrote) {
    const Catalog catalog = Catalog::parse(makeBytes(), 2);
    EXPECT_EQ(catalog.getFileCount(), 5U);
    EXPECT_EQ(catalog.getChunkCount(), 3U);
    EXPECT_EQ(catalog.getPartCount(), 6U);
    EXPECT_EQ(makeBytes(), makeBytes()) << "A catalog always has the same bytes for the same files.";

    const Catalog::File island = catalog.getFile(*catalog.findFile("content/maps/island.tmj"));
    EXPECT_EQ(island.path, "content/maps/island.tmj");
    EXPECT_EQ(island.size, 2500U);
    EXPECT_EQ(island.partCount, 3U);
    EXPECT_EQ(island.delivery, Delivery::Required);
    EXPECT_EQ(catalog.getPart(island.firstPart + 2).offset, 1500U);
    EXPECT_EQ(catalog.getChunk(catalog.getPart(island.firstPart + 1).chunk).shard, 1U);
    EXPECT_EQ(catalog.getFile(*catalog.findFile("content/maps/cave.tmj")).delivery, Delivery::OnDemand);
    EXPECT_EQ(catalog.getFile(*catalog.findFile("content/empty.txt")).size, 0U);
}

TEST_F(ContentCatalogTest, FindsFilesAndFolders) {
    const Catalog catalog = Catalog::parse(makeBytes(), 2);
    EXPECT_FALSE(catalog.findFile("content/maps").has_value());
    EXPECT_FALSE(catalog.findFile("content/maps/island").has_value());
    EXPECT_FALSE(catalog.findFile("content/zzz").has_value());

    EXPECT_EQ(listFolder(catalog, "content/maps"), (std::vector<std::string>{"content/maps/cave.tmj", "content/maps/island.tmj", "content/maps/tiles/grass.png"})) << "A folder never includes a sibling that shares its prefix.";
    EXPECT_EQ(listFolder(catalog, "content/maps/tiles"), (std::vector<std::string>{"content/maps/tiles/grass.png"}));
    EXPECT_TRUE(listFolder(catalog, "content/map").empty());
    EXPECT_EQ(listFolder(catalog, "").size(), 5U);
    EXPECT_TRUE(listFolder(catalog, "content/maps/island.tmj").empty());
}

TEST_F(ContentCatalogTest, FindsThePartThatHoldsAnOffset) {
    const Catalog catalog = Catalog::parse(makeBytes(), 2);
    const Catalog::File island = catalog.getFile(*catalog.findFile("content/maps/island.tmj"));
    EXPECT_EQ(catalog.findPart(island, 0), island.firstPart);
    EXPECT_EQ(catalog.findPart(island, 999), island.firstPart);
    EXPECT_EQ(catalog.findPart(island, 1000), island.firstPart + 1);
    EXPECT_EQ(catalog.findPart(island, 1499), island.firstPart + 1);
    EXPECT_EQ(catalog.findPart(island, 2499), island.firstPart + 2);
}

TEST_F(ContentCatalogTest, DescribesFilesLargerThanFourGigabytes) {
    // One chunk of the largest size repeats to make a file of 300 GiB, which the catalog describes in a few megabytes.
    const Catalog::Chunk chunk = makeChunk(1, 4 * 1024 * 1024);
    const std::uint64_t count = (std::uint64_t{300} << 30) / chunk.plainSize;
    CatalogWriter writer;
    writer.addChunk(chunk);
    writer.addFile("content/world.bin", Delivery::Required, std::vector<Digest>(static_cast<std::size_t>(count), chunk.storedId));
    const Catalog catalog = Catalog::parse(writer.write(), 1);

    const Catalog::File world = catalog.getFile(0);
    EXPECT_EQ(world.size, std::uint64_t{300} << 30);
    EXPECT_LT(catalog.getByteSize(), std::size_t{2} << 20);
    const std::uint64_t offset = (std::uint64_t{250} << 30) + 12345;
    const Catalog::Part part = catalog.getPart(catalog.findPart(world, offset));
    EXPECT_LE(part.offset, offset);
    EXPECT_GT(part.offset + chunk.plainSize, offset);
}

TEST_F(ContentCatalogTest, RejectsPathsThatAreNotNormalized) {
    for (const std::string& path : {std::string(""), std::string("/etc/passwd"), std::string("../secret"), std::string("content/../app.json"), std::string("content//a.png"), std::string("content/./a.png"), std::string("content/a.png/"), std::string("content\\a.png"), std::string("C:/a.png"), std::string("content/a\0b.png", 15), std::string("content/\x7F.png"), std::string("content/\xC0\xAF.png"), std::string("content/\xFF.png"), std::string(Catalog::kMaximumPathLength + 1, 'a')}) {
        EXPECT_FALSE(Catalog::isValidPath(path)) << path;
    }
    for (const std::string& path : {std::string("app.json"), std::string("content/maps/island.tmj"), std::string("content/ação/naïve €.png"), std::string(Catalog::kMaximumPathLength, 'a')}) {
        EXPECT_TRUE(Catalog::isValidPath(path)) << path;
    }

    CatalogWriter writer;
    const Catalog::Chunk chunk = makeChunk(1, 10);
    writer.addChunk(chunk);
    EXPECT_THROW(writer.addFile("content/../escape", Delivery::Required, {}), std::invalid_argument);
    EXPECT_THROW(writer.addFile("content/a", Delivery::Required, std::vector{makeChunk(2, 10).storedId}), std::invalid_argument);
    writer.addFile("content/a", Delivery::Required, {});
    EXPECT_THROW(writer.addFile("content/a", Delivery::Required, {}), std::invalid_argument);
}

TEST_F(ContentCatalogTest, RejectsDamagedCatalogs) {
    const std::vector<std::uint8_t> bytes = makeBytes();

    // Every truncation and every flipped byte either fails the checks or leaves a catalog that still reads consistently.
    for (std::size_t size = 0; size < bytes.size(); ++size) {
        expectCorrupt(std::vector<std::uint8_t>(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(size)));
    }
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        std::vector<std::uint8_t> changed = bytes;
        changed[index] ^= 0x80;
        try {
            const Catalog catalog = Catalog::parse(std::move(changed), 2);
            for (std::uint64_t file = 0; file < catalog.getFileCount(); ++file) {
                EXPECT_TRUE(Catalog::isValidPath(catalog.getFile(file).path));
            }
        } catch (const Error&) {}
    }

    std::vector<std::uint8_t> huge = bytes;
    setU64(huge, 8, std::uint64_t{1} << 62);
    expectCorrupt(huge);
    std::vector<std::uint8_t> wrapping = bytes;
    setU64(wrapping, 16, ~std::uint64_t{0} / 88 + 1);
    expectCorrupt(wrapping);
    expectCorrupt(bytes, 1);
}

TEST_F(ContentCatalogTest, RejectsUnsortedAndRepeatedEntries) {
    std::vector<std::uint8_t> swapped = makeBytes();
    std::swap_ranges(swapped.begin() + 64, swapped.begin() + 64 + Catalog::kFileSize, swapped.begin() + 64 + Catalog::kFileSize);
    expectCorrupt(swapped);

    std::vector<std::uint8_t> repeated = makeBytes();
    std::copy_n(repeated.begin() + 64, Catalog::kFileSize, repeated.begin() + 64 + Catalog::kFileSize);
    expectCorrupt(repeated);
}

} // namespace haylen::content
