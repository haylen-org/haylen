#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include "content/LuaCompiler.hpp"
#include "content/ReleasePackage.hpp"
#include "content/format/BinaryReader.hpp"
#include "content/format/BinaryWriter.hpp"
#include "content/format/CatalogWriter.hpp"
#include "content/format/ChannelDescriptor.hpp"
#include "content/format/ChannelWriter.hpp"
#include "content/format/ChunkRecord.hpp"
#include "content/format/PartialRecord.hpp"
#include "content/format/RecordSealer.hpp"
#include "content/format/ShardIndex.hpp"
#include "content/format/StoreEntry.hpp"
#include "content/format/StoreState.hpp"
#include "haylen/content/Error.hpp"
#include "haylen/lua/Application.hpp"
#include "support/ContentParsers.hpp"
#include "support/ReleaseFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::lua::content {

// Every parser of the content formats meets random bytes and damaged copies of valid files, and must reject them with a content error, never with a crash, an overread or another exception. The fuzzer of the content formats drives the same parsers without a limit.
class HostileInputTest : public ::testing::Test {
  protected:
    [[nodiscard]] static std::vector<std::uint8_t> withParser(std::uint8_t parser, std::vector<std::uint8_t> bytes) {
        bytes.insert(bytes.begin(), parser);
        return bytes;
    }

    // Valid inputs of every parser, each behind the byte that picks its parser.
    [[nodiscard]] std::vector<std::vector<std::uint8_t>> makeSeeds() {
        release.build({{"haylen.json", "{}"}, {"source/main.lua", "return 1"}, {"content/a.txt", "alpha"}, {"content/b.bin", std::string(5000, 'b')}});
        std::vector<std::vector<std::uint8_t>> seeds;

        const std::vector<std::uint8_t> plain = test::TestFiles::bytes("chunk");
        std::vector<std::uint8_t> ciphertext;
        const haylen::content::ChunkRecord::Header header = haylen::content::RecordSealer::seal(*release.getContentKey(), plain, haylen::content::ChunkRecord::identifyContent(plain), ciphertext).serialize();
        seeds.push_back(withParser(0, {header.begin(), header.end()}));

        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(release.getFolder())) {
            if (entry.path().extension() == ".hpak") {
                seeds.push_back(withParser(1, test::TestFiles::read(entry.path())));
            }
        }

        seeds.push_back(withParser(2, haylen::content::ShardIndex({{.storedId = haylen::content::Digest::of(plain), .recordOffset = 160, .encodedSize = 5, .plainSize = 5}, {.storedId = haylen::content::Digest::of(header), .recordOffset = 300, .encodedSize = 5, .plainSize = 5}}).serialize()));

        haylen::content::CatalogWriter catalog;
        catalog.addChunk({.storedId = haylen::content::Digest::of(plain), .contentId = haylen::content::Digest::of(header), .plainSize = 5, .encodedSize = 5, .shard = 3});
        catalog.addFile("content/a.txt", haylen::content::Delivery::Prefetch, std::vector{haylen::content::Digest::of(plain), haylen::content::Digest::of(plain)});
        catalog.addFile("content/b/c.txt", haylen::content::Delivery::Required, {});
        seeds.push_back(withParser(3, catalog.write()));

        seeds.push_back(withParser(4, test::TestFiles::read(release.getFolder() / haylen::content::ReleasePackage::kContentManifestFile)));
        const haylen::content::ChannelDescriptor descriptor{.application = haylen::content::Manifest::identifyApplication("com.haylen.tests"), .channel = "stable", .generation = 2};
        seeds.push_back(withParser(5, haylen::content::ChannelWriter::write(descriptor, release.getSigningKey())));
        seeds.push_back(withParser(6, haylen::content::StoreState{.application = descriptor.application, .channel = "stable", .acceptedGeneration = 2}.write()));
        const haylen::content::StoreEntry::Bytes entry = haylen::content::StoreEntry{.storedId = haylen::content::Digest::of(plain), .segment = 1, .offset = 16, .size = 200}.write();
        seeds.push_back(withParser(7, {entry.begin(), entry.end()}));
        seeds.push_back(withParser(8, haylen::content::PartialRecord{.shardId = haylen::content::Digest::of(plain), .recordOffset = 160, .recordSize = 300, .validator = "\"v\"", .bytes = plain}.write()));
        return seeds;
    }

    test::ReleaseFixture release{std::make_shared<lua::content::LuaCompiler>(), lua::Application::kSections};
};

TEST_F(HostileInputTest, ParsersRejectRandomBytes) {
    for (std::uint64_t seed = 0; seed < 3000; ++seed) {
        test::ContentParsers::parse(test::TestFiles::randomBytes(static_cast<std::size_t>(seed % 900), seed));
    }
}

TEST_F(HostileInputTest, ParsersRejectDamagedFiles) {
    for (const std::vector<std::uint8_t>& seed : makeSeeds()) {
        test::ContentParsers::parse(seed);
        const std::vector<std::uint8_t> noise = test::TestFiles::randomBytes(4000, seed.size());
        for (std::size_t round = 0; round < 500; ++round) {
            std::vector<std::uint8_t> damaged = seed;
            const std::size_t position = 1 + (std::size_t{noise[round]} * 257 + noise[round + 1] + round * 31) % (damaged.size() - 1);
            switch (round % 5) {
            case 0:
                damaged[position] ^= static_cast<std::uint8_t>(1U << (round % 8));
                break;
            case 1:
                damaged[position] = 0xFF;
                break;
            case 2:
                damaged.resize(position);
                break;
            case 3:
                std::fill_n(damaged.begin() + static_cast<std::ptrdiff_t>(position), std::min<std::size_t>(8, damaged.size() - position), std::uint8_t{0xFF});
                break;
            default:
                damaged.insert(damaged.begin() + static_cast<std::ptrdiff_t>(position), noise.begin() + static_cast<std::ptrdiff_t>(round), noise.begin() + static_cast<std::ptrdiff_t>(round + 16));
                break;
            }
            test::ContentParsers::parse(damaged);
        }
    }
}

TEST(BinaryReaderTest, NeverMovesPastItsBytes) {
    const std::vector<std::uint8_t> bytes = {2, 0, 'a', 'b', 0};
    haylen::content::BinaryReader reader(bytes, haylen::content::Error::Code::CorruptIndex, "test structure");
    try {
        reader.skip(bytes.size() + 1);
        FAIL() << "A skip past the end must fail.";
    } catch (const haylen::content::Error& error) {
        EXPECT_EQ(error.getCode(), haylen::content::Error::Code::CorruptIndex);
        EXPECT_EQ(std::string(error.what()), "The test structure ends before its fields do.");
    }

    EXPECT_EQ(reader.getPosition(), 0U);
    EXPECT_EQ(reader.readString(2), "ab");
    EXPECT_THROW(reader.requireEnd(), haylen::content::Error);
    reader.skipZeros(1);
    reader.requireEnd();
}

TEST(BinaryWriterTest, RefusesStringsLongerThanTheirLengthField) {
    haylen::content::BinaryWriter writer;
    EXPECT_THROW(writer.writeString(std::string(65536, 'a')), std::length_error);
    writer.writeString(std::string(65535, 'a'));
    EXPECT_EQ(writer.getBytes().size(), 65537U);
}

} // namespace haylen::lua::content
