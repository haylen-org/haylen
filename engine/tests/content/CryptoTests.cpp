#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "content/Error.hpp"
#include "content/crypto/Aead.hpp"
#include "content/crypto/ContentKey.hpp"
#include "content/crypto/Digest.hpp"
#include "content/crypto/KeyRing.hpp"
#include "content/crypto/SigningKey.hpp"
#include "content/crypto/VerifyingKey.hpp"
#include "content/format/ChunkRecord.hpp"
#include "content/format/Manifest.hpp"
#include "support/ReleaseFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::content {

class CryptoTest : public ::testing::Test {
  protected:
    static std::vector<std::uint8_t> fromHex(std::string_view text) {
        std::vector<std::uint8_t> bytes;
        for (std::size_t index = 0; index + 1 < text.size(); index += 2) {
            bytes.push_back(static_cast<std::uint8_t>(std::stoi(std::string(text.substr(index, 2)), nullptr, 16)));
        }
        return bytes;
    }

    static std::string toHex(std::span<const std::uint8_t> bytes) {
        static constexpr std::string_view kDigits = "0123456789abcdef";
        std::string text;
        for (const std::uint8_t byte : bytes) {
            text.push_back(kDigits[byte >> 4]);
            text.push_back(kDigits[byte & 0x0F]);
        }
        return text;
    }

    template <std::size_t Size> static std::array<std::uint8_t, Size> toArray(std::string_view text) {
        const std::vector<std::uint8_t> bytes = fromHex(text);
        std::array<std::uint8_t, Size> array{};
        std::copy_n(bytes.begin(), Size, array.begin());
        return array;
    }
};

TEST_F(CryptoTest, HashesWithBlake2b) {
    EXPECT_EQ(Digest::of({}).toHex(), "0e5751c026e543b2e8ab2eb06099daa1d1e5df47778f7787faab45cdf12fe3a8");
    EXPECT_EQ(Digest::of(test::TestFiles::bytes("abc")).toHex(), "bddd813c634239723171ef3fee98579b94964e3bb1cb3e427262c8c068d52319");
    EXPECT_TRUE(Digest().isZero());
    EXPECT_FALSE(Digest::of({}).isZero());
}

TEST_F(CryptoTest, IdentifiesChunksAndApps) {
    // These digests pin the identifiers of HPAK version 1, as an independent BLAKE2b computes them.
    const std::vector<std::uint8_t> chunk = test::TestFiles::bytes("abc");
    EXPECT_EQ(ChunkRecord::identifyContent(chunk).toHex(), "bddd813c634239723171ef3fee98579b94964e3bb1cb3e427262c8c068d52319");
    EXPECT_EQ(ChunkRecord::identifyStored(Compression::Codec::None, Compression::kNoneProfile, chunk).toHex(), "ccbae811613ba1735c5e2b578a784abcc83fcf6cd9bdf7a200cb8e88f315dc6a");
    EXPECT_EQ(ChunkRecord::identifyStored(Compression::Codec::Zstd, Compression::kZstdProfile, chunk).toHex(), "746f0c7ed05639ebf5d7fab9ac222bbfd2d7d6b603c1823f95b809a67fd53f2c");
    EXPECT_EQ(Manifest::identifyApplication("dev.haylen.tests").toHex(), "eca9d9e46ae5c626b9e92fc6f18bb5ea019e099af1507d50c9fa088303ab3def");
}

TEST_F(CryptoTest, EncryptsWithXChaCha20Poly1305) {
    const std::array<std::uint8_t, 32> key = toArray<32>("808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f");
    const Aead::Nonce nonce = toArray<24>("404142434445464748494a4b4c4d4e4f5051525354555657");
    const std::vector<std::uint8_t> additional = fromHex("50515253c0c1c2c3c4c5c6c7");
    const std::vector<std::uint8_t> plain = test::TestFiles::bytes("Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the future, sunscreen would be it.");

    std::vector<std::uint8_t> cipher(plain.size());
    const Aead::Tag tag = Aead::seal(key, nonce, additional, plain, cipher);
    EXPECT_EQ(toHex(cipher), "bd6d179d3e83d43b9576579493c0e939572a1700252bfaccbed2902c21396cbb731c7f1b0b4aa6440bf3a82f4eda7e39ae64c6708c54c216cb96b72e1213b4522f8c9ba40db5d945b11b69b982c1bb9e3f3fac2bc369488f76b2383565d3fff921f9664c97637da9768812f615c68b13b52e");
    EXPECT_EQ(toHex(tag), "c0875924c1c7987947deafd8780acf49");

    std::vector<std::uint8_t> opened(plain.size());
    ASSERT_TRUE(Aead::open(key, nonce, additional, tag, cipher, opened));
    EXPECT_EQ(opened, plain);
}

TEST_F(CryptoTest, RejectsAnyChangeToAuthenticatedData) {
    const std::array<std::uint8_t, 32> key = test::ReleaseFixture::makeKey(1);
    const Aead::Nonce nonce{};
    const std::vector<std::uint8_t> additional = test::TestFiles::bytes("header");
    const std::vector<std::uint8_t> plain = test::TestFiles::bytes("secret chunk");
    std::vector<std::uint8_t> cipher(plain.size());
    const Aead::Tag tag = Aead::seal(key, nonce, additional, plain, cipher);
    std::vector<std::uint8_t> opened(plain.size(), 0x55);

    Aead::Tag brokenTag = tag;
    brokenTag[0] ^= 1;
    EXPECT_FALSE(Aead::open(key, nonce, additional, brokenTag, cipher, opened));
    std::vector<std::uint8_t> brokenCipher = cipher;
    brokenCipher.back() ^= 1;
    EXPECT_FALSE(Aead::open(key, nonce, additional, tag, brokenCipher, opened));
    EXPECT_FALSE(Aead::open(key, nonce, test::TestFiles::bytes("headeR"), tag, cipher, opened));
    EXPECT_FALSE(Aead::open(test::ReleaseFixture::makeKey(2), nonce, additional, tag, cipher, opened));
    Aead::Nonce otherNonce{};
    otherNonce[23] = 1;
    EXPECT_FALSE(Aead::open(key, otherNonce, additional, tag, cipher, opened));
    EXPECT_EQ(opened, std::vector<std::uint8_t>(plain.size(), 0x55)) << "A failed decryption leaves its output untouched.";
}

TEST_F(CryptoTest, SignsWithEd25519) {
    // The first test vector of RFC 8032.
    const std::array<std::uint8_t, 32> seed = toArray<32>("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60");
    const SigningKey signingKey(seed);
    EXPECT_EQ(toHex(signingKey.getVerifyingKey().getBytes()), "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
    const VerifyingKey::Signature signature = signingKey.sign({});
    EXPECT_EQ(toHex(signature), "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b");
    EXPECT_TRUE(signingKey.getVerifyingKey().verify({}, signature));
}

TEST_F(CryptoTest, RejectsWrongSignaturesAndKeys) {
    const SigningKey signingKey(test::ReleaseFixture::makeKey(101));
    const SigningKey otherKey(test::ReleaseFixture::makeKey(102));
    const std::vector<std::uint8_t> message = test::TestFiles::bytes("manifest envelope");
    const VerifyingKey::Signature signature = signingKey.sign(message);

    EXPECT_TRUE(signingKey.getVerifyingKey().verify(message, signature));
    EXPECT_FALSE(otherKey.getVerifyingKey().verify(message, signature));
    EXPECT_FALSE(signingKey.getVerifyingKey().verify(test::TestFiles::bytes("manifest envelopf"), signature));
    for (std::size_t index = 0; index < signature.size(); index += 7) {
        VerifyingKey::Signature broken = signature;
        broken[index] ^= 0x20;
        EXPECT_FALSE(signingKey.getVerifyingKey().verify(message, broken)) << "Byte " << index;
    }
    EXPECT_NE(signingKey.getVerifyingKey().getId(), otherKey.getVerifyingKey().getId());
}

TEST_F(CryptoTest, DerivesStableSeparateSubkeys) {
    const ContentKey key(test::ReleaseFixture::makeKey(1));

    // These values pin the key derivation of HPAK version 1, which every shipped package depends on, as an independent BLAKE2b computes them.
    EXPECT_EQ(key.getId().toHex(), "37dc00650ca69de319784a4bf325e4b0d201b90bfe73cbe042aa7e6ea2efb6b7");
    EXPECT_EQ(toHex(key.getSubkey(ContentKey::Purpose::Chunk)), "3a17633f3f46cedbcf92e2602a14704c5cf1a284599139ff5f67a9b6d9e45eb2");

    std::set<std::string> subkeys;
    for (const ContentKey::Purpose purpose : {ContentKey::Purpose::Chunk, ContentKey::Purpose::ShardIndex, ContentKey::Purpose::Catalog, ContentKey::Purpose::Nonce}) {
        subkeys.insert(toHex(key.getSubkey(purpose)));
    }
    EXPECT_EQ(subkeys.size(), 4U) << "Every purpose has a key of its own.";
    EXPECT_NE(ContentKey(test::ReleaseFixture::makeKey(2)).getId(), key.getId());
}

TEST_F(CryptoTest, DerivesNoncesFromTheWholeMessage) {
    const ContentKey key(test::ReleaseFixture::makeKey(1));
    const std::vector<std::uint8_t> identity = test::TestFiles::bytes("record header and payload digest");
    const Aead::Nonce nonce = key.deriveNonce(ContentKey::Purpose::Chunk, identity);

    EXPECT_EQ(toHex(nonce), "8d47fd6a329dfb7c2606b5a3c7413674e3316a3e965f31ad");
    EXPECT_EQ(key.deriveNonce(ContentKey::Purpose::Chunk, identity), nonce) << "The same message always gets the same nonce.";
    EXPECT_NE(key.deriveNonce(ContentKey::Purpose::Catalog, identity), nonce) << "Purposes never share nonces.";
    EXPECT_NE(ContentKey(test::ReleaseFixture::makeKey(2)).deriveNonce(ContentKey::Purpose::Chunk, identity), nonce);

    // A change anywhere in the message gives another nonce.
    std::set<std::string> nonces{toHex(nonce)};
    for (std::size_t index = 0; index < identity.size(); ++index) {
        std::vector<std::uint8_t> changed = identity;
        changed[index] ^= 1;
        nonces.insert(toHex(key.deriveNonce(ContentKey::Purpose::Chunk, changed)));
    }
    nonces.insert(toHex(key.deriveNonce(ContentKey::Purpose::Chunk, std::span(identity).first(identity.size() - 1))));
    EXPECT_EQ(nonces.size(), identity.size() + 2);
}

TEST_F(CryptoTest, FindsKeysByIdForRotation) {
    KeyRing keys;
    const Digest first = keys.add(test::ReleaseFixture::makeKey(1));
    const Digest second = keys.add(test::ReleaseFixture::makeKey(2));
    EXPECT_EQ(keys.add(test::ReleaseFixture::makeKey(1)), first);
    EXPECT_TRUE(keys.contains(first));
    EXPECT_EQ(keys.get(second)->getId(), second);

    try {
        (void)keys.get(Digest::of(test::TestFiles::bytes("unknown")));
        FAIL() << "An unknown key ID must throw.";
    } catch (const Error& error) {
        EXPECT_EQ(error.getCode(), Error::Code::UnknownKeyId);
        EXPECT_NE(std::string(error.what()).find(Digest::of(test::TestFiles::bytes("unknown")).toHex()), std::string::npos);
    }
}

} // namespace haylen::content
