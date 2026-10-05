#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "content/Error.hpp"
#include "content/ShardReader.hpp"
#include "content/ShardWriter.hpp"
#include "content/crypto/KeyRing.hpp"
#include "content/format/ChunkRecord.hpp"
#include "content/format/ShardHeader.hpp"
#include "content/format/ShardIndex.hpp"
#include "io/MemoryReader.hpp"
#include "support/ReleaseFixture.hpp"
#include "support/TemporaryDirectory.hpp"
#include "support/TestFiles.hpp"

namespace haylen::content {

class ShardTest : public ::testing::Test {
  protected:
    // A file that holds a few regions of bytes and zeros everywhere else, as large as a test wants without the memory or the disk.
    class SparseReader final : public io::PackageReader {
      public:
        SparseReader(std::uint64_t fileSize, std::map<std::uint64_t, std::vector<std::uint8_t>> fileRegions) : size(fileSize), regions(std::move(fileRegions)) {}

        [[nodiscard]] std::uint64_t getSize() const noexcept override {
            return size;
        }
        [[nodiscard]] std::size_t read(std::uint64_t offset, std::span<std::uint8_t> target) override {
            const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(target.size(), offset < size ? size - offset : 0));
            std::fill_n(target.begin(), count, std::uint8_t{0});
            for (const auto& [start, bytes] : regions) {
                for (std::size_t index = 0; index < bytes.size(); ++index) {
                    if (start + index >= offset && start + index < offset + count) {
                        target[static_cast<std::size_t>(start + index - offset)] = bytes[index];
                    }
                }
            }
            return count;
        }

      private:
        std::uint64_t size;
        std::map<std::uint64_t, std::vector<std::uint8_t>> regions;
    };

    ShardTest() {
        keys.add(test::ReleaseFixture::makeKey(1));
    }

    [[nodiscard]] std::shared_ptr<const ContentKey> getKey() const {
        return keys.get(ContentKey(test::ReleaseFixture::makeKey(1)).getId());
    }

    [[nodiscard]] static std::vector<std::uint8_t> readFile(const std::filesystem::path& file) {
        std::ifstream stream(file, std::ios::binary);
        return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    }

    [[nodiscard]] static std::unique_ptr<io::PackageReader> makeReader(std::vector<std::uint8_t> bytes) {
        return std::make_unique<io::MemoryReader>(std::make_shared<const std::vector<std::uint8_t>>(std::move(bytes)));
    }

    // Writes a shard of chunks into a folder and returns its reference.
    [[nodiscard]] ShardReference writeShard(const std::filesystem::path& folder, const std::vector<std::vector<std::uint8_t>>& chunks, std::shared_ptr<const ContentKey> key) const {
        ShardWriter writer(folder, std::move(key));
        for (const std::vector<std::uint8_t>& chunk : chunks) {
            std::vector<std::uint8_t> ciphertext;
            writer.add(ChunkRecord::seal(*getKey(), chunk, ChunkRecord::identifyContent(chunk), ciphertext), ciphertext);
        }
        return writer.finish();
    }

    [[nodiscard]] static std::vector<std::vector<std::uint8_t>> makeChunks() {
        std::vector<std::vector<std::uint8_t>> chunks;
        for (std::uint64_t seed = 1; seed <= 4; ++seed) {
            chunks.push_back(test::TestFiles::randomBytes(1000 * seed, seed));
        }
        chunks.push_back(test::TestFiles::bytes(std::string(5000, 'a')));
        chunks.push_back(chunks.front());
        return chunks;
    }

    // Builds a shard around an index that is sealed properly, so only the checks of the index itself can reject it.
    [[nodiscard]] std::pair<std::vector<std::uint8_t>, ShardReference> makeShard(std::vector<std::uint8_t> index, std::uint64_t entries, std::uint64_t dataSize) const {
        ShardHeader header;
        header.shardId = Digest::of(test::TestFiles::bytes("shard"));
        header.keyId = getKey()->getId();
        header.indexOffset = ShardHeader::kSize + dataSize;
        header.indexSize = index.size();
        header.entryCount = entries;
        header.sealIndex(*getKey(), index);
        std::vector<std::uint8_t> file(static_cast<std::size_t>(header.indexOffset));
        std::ranges::copy(header.serialize(), file.begin());
        file.insert(file.end(), index.begin(), index.end());
        const ShardReference reference{.shardId = header.shardId, .fileSize = file.size(), .fileDigest = Digest::of(file)};
        return {std::move(file), reference};
    }

    [[nodiscard]] static ShardIndex::Entry makeEntry(std::uint8_t id, std::uint64_t offset) {
        return {.storedId = Digest::of(std::vector<std::uint8_t>{id}), .recordOffset = offset, .encodedSize = 100, .plainSize = 100};
    }

    void expectRejectedWith(const std::pair<std::vector<std::uint8_t>, ShardReference>& shard, Error::Code code) const {
        try {
            const ShardReader reader(makeReader(shard.first), shard.second, keys);
            ADD_FAILURE() << "The index must be rejected.";
        } catch (const Error& error) {
            EXPECT_EQ(error.getCode(), code) << error.what();
        }
    }

    static void expectReadFails(const ShardReader& reader, const Digest& storedId, const Digest& contentId, Error::Code code) {
        std::vector<std::uint8_t> scratch;
        std::vector<std::uint8_t> plain;
        try {
            reader.readChunk(storedId, contentId, scratch, plain);
            ADD_FAILURE() << "The read must fail.";
        } catch (const Error& error) {
            EXPECT_EQ(error.getCode(), code);
        }
    }

    static void expectRejected(std::vector<std::uint8_t> bytes, const ShardReference& reference, const KeyRing& ring) {
        try {
            const ShardReader reader(makeReader(std::move(bytes)), reference, ring);
            ADD_FAILURE() << "The damaged shard must be rejected.";
        } catch (const Error&) {}
    }

    KeyRing keys;
    test::TemporaryDirectory directory;
};

TEST_F(ShardTest, SealsChunksDeterministically) {
    const std::vector<std::uint8_t> plain = test::TestFiles::bytes(std::string(10000, 'x'));
    std::vector<std::uint8_t> ciphertext;
    const ChunkRecord record = ChunkRecord::seal(*getKey(), plain, ChunkRecord::identifyContent(plain), ciphertext);
    EXPECT_EQ(record.codec, Compression::Codec::Zstd);
    EXPECT_EQ(record.plainSize, plain.size());
    EXPECT_EQ(record.encodedSize, ciphertext.size());
    EXPECT_EQ(record.contentId, Digest::of(plain));

    std::vector<std::uint8_t> again;
    EXPECT_EQ(ChunkRecord::seal(*getKey(), plain, record.contentId, again).serialize(), record.serialize());
    EXPECT_EQ(again, ciphertext) << "An unchanged chunk seals to the same bytes in every build.";

    std::vector<std::uint8_t> otherCiphertext;
    const std::vector<std::uint8_t> other = test::TestFiles::bytes(std::string(10000, 'y'));
    const ChunkRecord otherRecord = ChunkRecord::seal(*getKey(), other, ChunkRecord::identifyContent(other), otherCiphertext);
    EXPECT_NE(otherRecord.nonce, record.nonce);
    EXPECT_NE(otherRecord.storedId, record.storedId);

    const ChunkRecord parsed = ChunkRecord::parse(record.serialize());
    EXPECT_EQ(parsed.serialize(), record.serialize());
    std::vector<std::uint8_t> opened;
    parsed.open(*getKey(), ciphertext, opened);
    EXPECT_EQ(opened, plain);
}

TEST_F(ShardTest, RejectsEveryChangeToARecord) {
    const std::vector<std::uint8_t> plain = test::TestFiles::randomBytes(3000, 9);
    std::vector<std::uint8_t> ciphertext;
    const ChunkRecord record = ChunkRecord::seal(*getKey(), plain, ChunkRecord::identifyContent(plain), ciphertext);
    const ChunkRecord::Header header = record.serialize();

    for (std::size_t index = 0; index < ChunkRecord::kHeaderSize + ciphertext.size(); index += 3) {
        ChunkRecord::Header changedHeader = header;
        std::vector<std::uint8_t> changedCiphertext = ciphertext;
        if (index < ChunkRecord::kHeaderSize) {
            changedHeader[index] ^= 0x01;
        } else {
            changedCiphertext[index - ChunkRecord::kHeaderSize] ^= 0x01;
        }
        std::vector<std::uint8_t> opened;
        try {
            ChunkRecord::parse(changedHeader).open(*getKey(), changedCiphertext, opened);
            ADD_FAILURE() << "A change at byte " << index << " went unnoticed.";
        } catch (const Error&) {
            EXPECT_TRUE(opened.empty()) << "Unverified bytes never leave a record.";
        }
    }
}

TEST_F(ShardTest, ChecksTheContentIdOfDecodedChunks) {
    // A record sealed with the key under a content ID that does not match its bytes authenticates, but its bytes never reach the caller.
    const std::vector<std::uint8_t> plain = test::TestFiles::randomBytes(2000, 10);
    std::vector<std::uint8_t> ciphertext;
    const ChunkRecord forged = ChunkRecord::seal(*getKey(), plain, Digest::of(test::TestFiles::bytes("other")), ciphertext);
    std::vector<std::uint8_t> opened;
    try {
        forged.open(*getKey(), ciphertext, opened);
        FAIL() << "The forged chunk must be rejected.";
    } catch (const Error& error) {
        EXPECT_EQ(error.getCode(), Error::Code::ChunkHashMismatch);
        EXPECT_TRUE(opened.empty());
    }
}

TEST_F(ShardTest, WritesAndReadsShards) {
    const std::vector<std::vector<std::uint8_t>> chunks = makeChunks();
    const ShardReference reference = writeShard(directory.getPath(), chunks, getKey());
    const std::vector<std::uint8_t> file = readFile(directory.getPath() / reference.getFileName());
    EXPECT_EQ(reference.fileSize, file.size());
    EXPECT_EQ(reference.fileDigest, Digest::of(file));
    EXPECT_FALSE(std::filesystem::exists(directory.getPath() / "shard.hpak.partial"));

    const ShardReader reader(makeReader(file), reference, keys);
    EXPECT_EQ(reader.getHeader().shardId, reference.shardId);
    EXPECT_EQ(reader.getHeader().keyId, getKey()->getId());
    EXPECT_EQ(reader.getIndex().getEntries().size(), chunks.size() - 1) << "A shard stores a repeated chunk once.";

    std::vector<std::uint8_t> scratch;
    std::vector<std::uint8_t> plain;
    for (const std::vector<std::uint8_t>& chunk : chunks) {
        const Compression::Encoded encoded = Compression::encode(chunk);
        reader.readChunk(ChunkRecord::identifyStored(encoded.codec, encoded.profile, encoded.bytes), Digest::of(chunk), scratch, plain);
        EXPECT_EQ(plain, chunk);
    }
}

TEST_F(ShardTest, WritesTheSameBytesForTheSameChunks) {
    const std::vector<std::vector<std::uint8_t>> chunks = makeChunks();
    const ShardReference first = writeShard(directory.getPath() / "first", chunks, getKey());
    const ShardReference second = writeShard(directory.getPath() / "second", chunks, getKey());
    EXPECT_EQ(first, second);
    EXPECT_EQ(readFile(directory.getPath() / "first" / first.getFileName()), readFile(directory.getPath() / "second" / second.getFileName()));

    std::vector<std::vector<std::uint8_t>> changed = chunks;
    changed.back() = test::TestFiles::bytes("one more chunk");
    EXPECT_NE(writeShard(directory.getPath() / "third", changed, getKey()).shardId, first.shardId);
}

TEST_F(ShardTest, RejectsEveryChangeToTheHeaderAndTheIndex) {
    const ShardReference reference = writeShard(directory.getPath(), makeChunks(), getKey());
    const std::vector<std::uint8_t> file = readFile(directory.getPath() / reference.getFileName());
    const ShardHeader header = ShardHeader::parse(std::span(file).first<ShardHeader::kSize>(), file.size());

    for (std::size_t index = 0; index < ShardHeader::kSize; ++index) {
        std::vector<std::uint8_t> changed = file;
        changed[index] ^= 0x10;
        expectRejected(std::move(changed), reference, keys);
    }
    for (std::uint64_t index = header.indexOffset; index < file.size(); index += 5) {
        std::vector<std::uint8_t> changed = file;
        changed[static_cast<std::size_t>(index)] ^= 0x01;
        expectRejected(std::move(changed), reference, keys);
    }
    for (std::size_t size = 0; size < file.size(); size += 97) {
        expectRejected(std::vector<std::uint8_t>(file.begin(), file.begin() + static_cast<std::ptrdiff_t>(size)), reference, keys);
        ShardReference truncated = reference;
        truncated.fileSize = size;
        expectRejected(std::vector<std::uint8_t>(file.begin(), file.begin() + static_cast<std::ptrdiff_t>(size)), truncated, keys);
    }
}

TEST_F(ShardTest, RejectsIndexesThatPlaceRecordsBadly) {
    const std::uint64_t data = ShardHeader::kSize;
    expectRejectedWith(makeShard(ShardIndex({makeEntry(1, data), makeEntry(2, data + 140)}).serialize(), 2, 600), Error::Code::InvalidOffset);
    expectRejectedWith(makeShard(ShardIndex({makeEntry(1, data), makeEntry(2, data + 400)}).serialize(), 2, 400), Error::Code::InvalidOffset);
    expectRejectedWith(makeShard(ShardIndex({makeEntry(1, 10)}).serialize(), 1, 400), Error::Code::InvalidOffset);

    std::vector<std::uint8_t> unsorted = ShardIndex({makeEntry(1, data), makeEntry(2, data + 228)}).serialize();
    std::rotate(unsorted.begin(), unsorted.begin() + ShardIndex::kEntrySize, unsorted.end());
    expectRejectedWith(makeShard(unsorted, 2, 456), Error::Code::CorruptIndex);

    ShardIndex::Entry impossible = makeEntry(1, data);
    impossible.plainSize = 5 * 1024 * 1024;
    impossible.encodedSize = impossible.plainSize;
    expectRejectedWith(makeShard(ShardIndex({impossible}).serialize(), 1, 6 * 1024 * 1024), Error::Code::CorruptIndex);
    expectRejectedWith(makeShard(ShardIndex({makeEntry(1, data)}).serialize(), ShardIndex::kMaximumEntries + 1, 400), Error::Code::CorruptHeader);
}

TEST_F(ShardTest, ReadsRecordsBeyondFourGigabytes) {
    // The record sits past 5 GiB of a sparse file, so every offset of the shard needs 64 bits.
    const std::vector<std::uint8_t> plain = test::TestFiles::randomBytes(4096, 11);
    std::vector<std::uint8_t> ciphertext;
    const ChunkRecord record = ChunkRecord::seal(*getKey(), plain, Digest::of(plain), ciphertext);
    const std::uint64_t recordOffset = std::uint64_t{5} << 30;

    const ChunkRecord::Header recordHeader = record.serialize();
    std::vector<std::uint8_t> bytes(recordHeader.begin(), recordHeader.end());
    bytes.insert(bytes.end(), ciphertext.begin(), ciphertext.end());
    std::vector<std::uint8_t> index = ShardIndex({{.storedId = record.storedId, .recordOffset = recordOffset, .encodedSize = record.encodedSize, .plainSize = record.plainSize, .codec = record.codec, .profile = record.profile}}).serialize();

    ShardHeader header;
    header.shardId = Digest::of(test::TestFiles::bytes("sparse"));
    header.keyId = getKey()->getId();
    header.indexOffset = recordOffset + bytes.size();
    header.indexSize = index.size();
    header.entryCount = 1;
    header.sealIndex(*getKey(), index);
    const ShardHeader::Bytes headerBytes = header.serialize();

    const std::uint64_t size = header.indexOffset + header.indexSize;
    std::map<std::uint64_t, std::vector<std::uint8_t>> regions{{0, {headerBytes.begin(), headerBytes.end()}}, {recordOffset, bytes}, {header.indexOffset, index}};
    const ShardReader reader(std::make_unique<SparseReader>(size, std::move(regions)), {.shardId = header.shardId, .fileSize = size}, keys);
    std::vector<std::uint8_t> scratch;
    std::vector<std::uint8_t> read;
    reader.readChunk(record.storedId, record.contentId, scratch, read);
    EXPECT_EQ(read, plain);
}

TEST_F(ShardTest, OpensEachShardWithItsOwnKey) {
    KeyRing rotated;
    rotated.add(test::ReleaseFixture::makeKey(2));
    const std::shared_ptr<const ContentKey> second = rotated.get(ContentKey(test::ReleaseFixture::makeKey(2)).getId());
    const ShardReference reference = writeShard(directory.getPath(), makeChunks(), second);
    const std::vector<std::uint8_t> file = readFile(directory.getPath() / reference.getFileName());

    try {
        const ShardReader reader(makeReader(file), reference, keys);
        FAIL() << "A shard under a key the app lacks must be rejected.";
    } catch (const Error& error) {
        EXPECT_EQ(error.getCode(), Error::Code::UnknownKeyId);
    }

    keys.add(test::ReleaseFixture::makeKey(2));
    const ShardReader reader(makeReader(file), reference, keys);
    EXPECT_EQ(reader.getHeader().keyId, second->getId());
}

TEST_F(ShardTest, ReportsChunksTheShardDoesNotHold) {
    const std::vector<std::vector<std::uint8_t>> chunks = makeChunks();
    const ShardReference reference = writeShard(directory.getPath(), chunks, getKey());
    const ShardReader reader(makeReader(readFile(directory.getPath() / reference.getFileName())), reference, keys);
    const Digest stored = reader.getIndex().getEntries().front().storedId;
    expectReadFails(reader, Digest::of(test::TestFiles::bytes("missing")), Digest(), Error::Code::MissingChunk);
    expectReadFails(reader, stored, Digest::of(test::TestFiles::bytes("another chunk")), Error::Code::CorruptChunk);
}

} // namespace haylen::content
