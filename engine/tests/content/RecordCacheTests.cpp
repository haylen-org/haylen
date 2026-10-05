#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "content/RecordCache.hpp"
#include "content/crypto/ContentKey.hpp"
#include "content/format/ChunkRecord.hpp"
#include "support/ReleaseFixture.hpp"
#include "support/TemporaryDirectory.hpp"
#include "support/TestFiles.hpp"

namespace haylen::content {

class RecordCacheTest : public ::testing::Test {
  protected:
    RecordCacheTest() : key(test::ReleaseFixture::makeKey(1)), otherKey(test::ReleaseFixture::makeKey(2)), cache(directory.getPath()) {
        plain = test::TestFiles::bytes("A chunk of an asset that repeats, repeats, repeats and repeats until it compresses.");
        record = ChunkRecord::seal(key, plain, ChunkRecord::identifyContent(plain), ciphertext);
    }

    // The single entry of the cache, wherever its key and encoder place it.
    [[nodiscard]] std::filesystem::path findEntry() const {
        for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(directory.getPath())) {
            if (entry.is_regular_file()) {
                return entry.path();
            }
        }
        return {};
    }

    test::TemporaryDirectory directory;
    ContentKey key;
    ContentKey otherKey;
    RecordCache cache;
    std::vector<std::uint8_t> plain;
    std::vector<std::uint8_t> ciphertext;
    ChunkRecord record;
};

TEST_F(RecordCacheTest, ServesTheRecordsItStored) {
    std::vector<std::uint8_t> found;
    EXPECT_FALSE(cache.find(key, record.contentId, found).has_value());
    cache.store(key, record, ciphertext);

    const std::optional<ChunkRecord> cached = cache.find(key, record.contentId, found);
    ASSERT_TRUE(cached.has_value());
    EXPECT_EQ(cached->serialize(), record.serialize());
    EXPECT_EQ(found, ciphertext);

    // The cache holds only the sealed record, never the plain chunk.
    std::ifstream stream(findEntry(), std::ios::binary);
    const std::string stored{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    EXPECT_EQ(stored.find("repeats"), std::string::npos);
}

TEST_F(RecordCacheTest, IgnoresEntriesThatDoNotVerify) {
    cache.store(key, record, ciphertext);
    std::vector<std::uint8_t> found;
    EXPECT_FALSE(cache.find(otherKey, record.contentId, found).has_value()) << "Another key never finds the records of this one.";

    // An entry that holds the record of another chunk, or bytes after its record, never serves.
    const std::vector<std::uint8_t> otherPlain = test::TestFiles::bytes("Another chunk of another asset.");
    std::vector<std::uint8_t> otherCiphertext;
    const ChunkRecord::Header otherHeader = ChunkRecord::seal(key, otherPlain, ChunkRecord::identifyContent(otherPlain), otherCiphertext).serialize();
    std::ofstream(findEntry(), std::ios::binary | std::ios::trunc).write(reinterpret_cast<const char*>(otherHeader.data()), static_cast<std::streamsize>(otherHeader.size())).write(reinterpret_cast<const char*>(otherCiphertext.data()), static_cast<std::streamsize>(otherCiphertext.size()));
    EXPECT_FALSE(cache.find(key, record.contentId, found).has_value());
    cache.store(key, record, ciphertext);
    std::ofstream(findEntry(), std::ios::binary | std::ios::app).put('x');
    EXPECT_FALSE(cache.find(key, record.contentId, found).has_value());
    cache.store(key, record, ciphertext);
    ASSERT_TRUE(cache.find(key, record.contentId, found).has_value());

    std::fstream stream(findEntry(), std::ios::binary | std::ios::in | std::ios::out);
    stream.seekp(static_cast<std::streamoff>(ChunkRecord::kHeaderSize + 2));
    stream.put('x');
    stream.close();
    EXPECT_FALSE(cache.find(key, record.contentId, found).has_value()) << "A damaged entry is sealed again instead of serving.";

    std::filesystem::resize_file(findEntry(), 10);
    EXPECT_FALSE(cache.find(key, record.contentId, found).has_value());
}

TEST_F(RecordCacheTest, ReportsAnEntryItCannotWrite) {
    // A folder under the name the entry grows in leaves no room for it, and the store stops with the name of the entry.
    cache.store(key, record, ciphertext);
    std::filesystem::path partial = findEntry();
    std::filesystem::remove(partial);
    partial += ".partial";
    std::filesystem::create_directories(partial);
    try {
        cache.store(key, record, ciphertext);
        FAIL() << "An entry that cannot be written must stop the store.";
    } catch (const std::runtime_error& error) {
        EXPECT_EQ(std::string(error.what()), "The build cache entry \"" + partial.generic_string() + "\" could not be written.");
    }
}

} // namespace haylen::content
