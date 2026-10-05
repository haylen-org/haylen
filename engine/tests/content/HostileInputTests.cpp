#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "content/ReleasePackage.hpp"
#include "content/format/CatalogWriter.hpp"
#include "content/format/ChannelDescriptor.hpp"
#include "content/format/ChunkRecord.hpp"
#include "content/format/ShardIndex.hpp"
#include "support/ContentParsers.hpp"
#include "support/ReleaseFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::content {

// Every parser of the content formats meets random bytes and damaged copies of valid files, and must reject them with a content error, never with a crash, an overread or another exception. The fuzzer of the content formats drives the same parsers without a limit.
class HostileInputTest : public ::testing::Test {
  protected:
    [[nodiscard]] static std::vector<std::uint8_t> withParser(std::uint8_t parser, std::vector<std::uint8_t> bytes) {
        bytes.insert(bytes.begin(), parser);
        return bytes;
    }

    [[nodiscard]] static std::vector<std::uint8_t> readFile(const std::filesystem::path& file) {
        std::ifstream stream(file, std::ios::binary);
        return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    }

    // Valid inputs of every parser, each behind the byte that picks its parser.
    [[nodiscard]] std::vector<std::vector<std::uint8_t>> makeSeeds() {
        release.build({{"app.json", "{}"}, {"source/main.lua", "return 1"}, {"content/a.txt", "alpha"}, {"content/b.bin", std::string(5000, 'b')}});
        std::vector<std::vector<std::uint8_t>> seeds;

        const std::vector<std::uint8_t> plain = test::TestFiles::bytes("chunk");
        std::vector<std::uint8_t> ciphertext;
        const ChunkRecord::Header header = ChunkRecord::seal(*release.getContentKey(), plain, ChunkRecord::identifyContent(plain), ciphertext).serialize();
        seeds.push_back(withParser(0, {header.begin(), header.end()}));

        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(release.getFolder())) {
            if (entry.path().extension() == ".hpak") {
                seeds.push_back(withParser(1, readFile(entry.path())));
            }
        }
        seeds.push_back(withParser(2, ShardIndex({{.storedId = Digest::of(plain), .recordOffset = 160, .encodedSize = 5, .plainSize = 5}, {.storedId = Digest::of(header), .recordOffset = 300, .encodedSize = 5, .plainSize = 5}}).serialize()));

        CatalogWriter catalog;
        catalog.addChunk({.storedId = Digest::of(plain), .contentId = Digest::of(header), .plainSize = 5, .encodedSize = 5, .shard = 3});
        catalog.addFile("content/a.txt", Delivery::Prefetch, std::vector{Digest::of(plain), Digest::of(plain)});
        catalog.addFile("content/b/c.txt", Delivery::Required, {});
        seeds.push_back(withParser(3, catalog.write()));

        seeds.push_back(withParser(4, readFile(release.getFolder() / ReleasePackage::kContentManifestFile)));
        const ChannelDescriptor descriptor{.application = Manifest::identifyApplication("dev.haylen.tests"), .channel = "stable", .generation = 2};
        seeds.push_back(withParser(5, descriptor.write(release.getSigningKey())));
        return seeds;
    }

    test::ReleaseFixture release;
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

} // namespace haylen::content
