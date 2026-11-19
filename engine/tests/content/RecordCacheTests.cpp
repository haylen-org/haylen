#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "content/RecordCache.hpp"
#include "content/crypto/ContentKey.hpp"
#include "content/format/ChunkRecord.hpp"
#include "content/format/RecordSealer.hpp"
#include "support/ReleaseFixture.hpp"
#include "support/TemporaryDirectory.hpp"
#include "support/TestFiles.hpp"

namespace haylen::content {

class RecordCacheTest : public ::testing::Test {
  protected:
    RecordCacheTest() : key(test::ReleaseFixture::makeKey(1)), otherKey(test::ReleaseFixture::makeKey(2)), cache(directory.getPath()) {
        plain = test::TestFiles::bytes("A chunk of an asset that repeats, repeats, repeats and repeats until it compresses.");
        record = RecordSealer::seal(key, plain, ChunkRecord::identifyContent(plain), ciphertext);
    }

    // The single entry of the cache, wherever its key and encoder place it.
    [[nodiscard]] std::filesystem::path findEntry() const {
        for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(directory.getPath())) {
            if (entry.is_regular_file()) {
                return entry.path();
            }
        }

        throw std::runtime_error("The cache holds no entry.");
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
    const std::string stored = test::TestFiles::readText(findEntry());
    EXPECT_EQ(stored.find("repeats"), std::string::npos);
}

TEST_F(RecordCacheTest, IgnoresEntriesThatDoNotVerify) {
    cache.store(key, record, ciphertext);
    std::vector<std::uint8_t> found;
    EXPECT_FALSE(cache.find(otherKey, record.contentId, found).has_value()) << "Another key never finds the records of this one.";

    // An entry that holds the record of another chunk, or bytes after its record, never serves.
    const std::vector<std::uint8_t> otherPlain = test::TestFiles::bytes("Another chunk of another asset.");
    std::vector<std::uint8_t> otherCiphertext;
    const ChunkRecord::Header otherHeader = RecordSealer::seal(key, otherPlain, ChunkRecord::identifyContent(otherPlain), otherCiphertext).serialize();
    std::vector<std::uint8_t> otherEntry(otherHeader.begin(), otherHeader.end());
    otherEntry.insert(otherEntry.end(), otherCiphertext.begin(), otherCiphertext.end());
    test::TestFiles::write(findEntry(), otherEntry);
    EXPECT_FALSE(cache.find(key, record.contentId, found).has_value());
    cache.store(key, record, ciphertext);
    test::TestFiles::append(findEntry(), "x");
    EXPECT_FALSE(cache.find(key, record.contentId, found).has_value());
    cache.store(key, record, ciphertext);
    ASSERT_TRUE(cache.find(key, record.contentId, found).has_value());

    test::TestFiles::damage(findEntry(), ChunkRecord::kHeaderSize + 2);
    EXPECT_FALSE(cache.find(key, record.contentId, found).has_value()) << "A damaged entry is sealed again instead of serving.";

    std::filesystem::resize_file(findEntry(), 10);
    EXPECT_FALSE(cache.find(key, record.contentId, found).has_value());
}

TEST_F(RecordCacheTest, ReportsAnEntryItCannotWrite) {
    // A folder under the name the entry grows in leaves no room for it, and the store stops with the name of the entry.
    cache.store(key, record, ciphertext);
    std::filesystem::path partial = findEntry();
    ASSERT_TRUE(std::filesystem::remove(partial));
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
