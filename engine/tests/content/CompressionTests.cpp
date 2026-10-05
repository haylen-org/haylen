#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "content/Error.hpp"
#include "content/format/Compression.hpp"
#include "support/TestFiles.hpp"

namespace haylen::content {

class CompressionTest : public ::testing::Test {
  protected:
    static std::vector<std::uint8_t> makeText() {
        std::string text;
        for (int line = 0; line < 4000; ++line) {
            text += "{\"tile\": " + std::to_string(line % 37) + ", \"layer\": \"ground\", \"solid\": true}\n";
        }
        return test::TestFiles::bytes(text);
    }

    static void expectCorrupt(Compression::Codec codec, std::span<const std::uint8_t> encoded, std::size_t plainSize) {
        std::vector<std::uint8_t> plain(plainSize);
        try {
            Compression::decode(codec, encoded, plain);
            FAIL() << "Decoding must fail.";
        } catch (const Error& error) {
            EXPECT_EQ(error.getCode(), Error::Code::CorruptChunk);
        }
    }
};

TEST_F(CompressionTest, CompressesOnlyWhenItSaves) {
    const std::vector<std::uint8_t> text = makeText();
    const Compression::Encoded compressed = Compression::encode(text);
    EXPECT_EQ(compressed.codec, Compression::Codec::Zstd);
    EXPECT_EQ(compressed.profile, Compression::kZstdProfile);
    EXPECT_LT(compressed.bytes.size(), text.size() / 10);
    EXPECT_EQ(Compression::encode(text).bytes, compressed.bytes) << "The same chunk always compresses to the same bytes.";

    // Data that is compressed already, which random bytes stand for, is stored as it is.
    const std::vector<std::uint8_t> random = test::TestFiles::randomBytes(300000, 1);
    const Compression::Encoded stored = Compression::encode(random);
    EXPECT_EQ(stored.codec, Compression::Codec::None);
    EXPECT_EQ(stored.profile, Compression::kNoneProfile);
    EXPECT_EQ(stored.bytes, random);
    EXPECT_EQ(Compression::encode(test::TestFiles::bytes("tiny")).codec, Compression::Codec::None);
}

TEST_F(CompressionTest, DecodesExactlyTheDeclaredSize) {
    const std::vector<std::uint8_t> text = makeText();
    const Compression::Encoded compressed = Compression::encode(text);
    std::vector<std::uint8_t> plain(text.size());
    Compression::decode(compressed.codec, compressed.bytes, plain);
    EXPECT_EQ(plain, text);

    // A frame that would decode to more or fewer bytes than declared never decodes, which stops decompression bombs.
    expectCorrupt(Compression::Codec::Zstd, compressed.bytes, text.size() - 1);
    expectCorrupt(Compression::Codec::Zstd, compressed.bytes, text.size() + 1);
    expectCorrupt(Compression::Codec::None, text, text.size() + 1);
}

TEST_F(CompressionTest, RejectsDamagedFrames) {
    const std::vector<std::uint8_t> text = makeText();
    const Compression::Encoded compressed = Compression::encode(text);
    expectCorrupt(Compression::Codec::Zstd, std::span(compressed.bytes).first(compressed.bytes.size() / 2), text.size());
    expectCorrupt(Compression::Codec::Zstd, test::TestFiles::bytes("not a zstd frame at all"), text.size());

    std::vector<std::uint8_t> damaged = compressed.bytes;
    damaged[damaged.size() / 2] ^= 0xFF;
    std::vector<std::uint8_t> plain(text.size());
    try {
        Compression::decode(Compression::Codec::Zstd, damaged, plain);
        EXPECT_NE(plain, text) << "A damaged frame either fails or decodes to other bytes, which the content ID check of a record catches.";
    } catch (const Error& error) {
        EXPECT_EQ(error.getCode(), Error::Code::CorruptChunk);
    }
}

TEST_F(CompressionTest, KnowsItsCodecsAndProfiles) {
    EXPECT_TRUE(Compression::isSupported(0, 0));
    EXPECT_TRUE(Compression::isSupported(1, 1));
    EXPECT_FALSE(Compression::isSupported(0, 1));
    EXPECT_FALSE(Compression::isSupported(1, 0));
    EXPECT_FALSE(Compression::isSupported(2, 1));
}

} // namespace haylen::content
