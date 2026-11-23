#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <iterator>
#include <string>
#include <vector>

#include "content/BootstrapWriter.hpp"
#include "content/KeyStore.hpp"
#include "content/crypto/Digest.hpp"
#include "haylen/content/EmbeddedKeyProvider.hpp"
#include "support/TemporaryDirectory.hpp"
#include "support/TestFiles.hpp"

namespace haylen::lua::content {

class BootstrapWriterTest : public ::testing::Test {
  protected:
    // Reads the 32 bytes of an array literal that follows a field name in the source.
    [[nodiscard]] static std::array<std::uint8_t, 32> readArray(const std::string& source, const std::string& field, std::size_t occurrence = 0) {
        std::size_t start = 0;
        for (std::size_t index = 0; index <= occurrence; ++index) {
            start = source.find("." + field + " = {", start) + field.size() + 5;
        }

        std::array<std::uint8_t, 32> bytes{};
        const std::string text = source.substr(start, source.find('}', start) - start);
        std::size_t position = 0;
        for (std::uint8_t& byte : bytes) {
            position = text.find("0x", position) + 2;
            byte = static_cast<std::uint8_t>(std::stoi(text.substr(position, 2), nullptr, 16));
        }

        return bytes;
    }

    // Writes bytes as the literals of the source, which a key must never appear as.
    [[nodiscard]] static std::string writeLiterals(const std::vector<std::uint8_t>& bytes) {
        std::string text;
        for (const std::uint8_t byte : bytes) {
            text += std::format("{}0x{:02x}", text.empty() ? "" : ", ", byte);
        }

        return text;
    }

    test::TemporaryDirectory directory;
};

TEST_F(BootstrapWriterTest, WritesTheIdentityAndSealedKeysOfTheApp) {
    haylen::content::KeyStore keys = haylen::content::KeyStore::create(directory.getPath() / "keys", "com.haylen.tests");
    keys.rotate();
    const std::string source = haylen::content::BootstrapWriter::write(keys, "apple", 1002003, "lua-5.5-f0-i4-x4-l8-n8-le");
    EXPECT_EQ(haylen::content::BootstrapWriter::write(keys, "apple", 1002003, "lua-5.5-f0-i4-x4-l8-n8-le"), source) << "The same keys always write the same bootstrap.";
    EXPECT_NE(source.find(".identifier = \"com.haylen.tests\", .appBuild = 1002003U, .profile = \"apple\", .luaAbi = \"lua-5.5-f0-i4-x4-l8-n8-le\""), std::string::npos) << source;
    EXPECT_NE(source.find("const bool HaylenBootstrap::kInstalled = HaylenBootstrap::install();"), std::string::npos);

    // Each sealed key opens to the key of its file, while the source holds neither a key nor the seed of the signing key in any form.
    for (std::size_t index = 0; index < keys.getContentKeyIds().size(); ++index) {
        const haylen::content::Digest& id = keys.getContentKeyIds()[index];
        const haylen::content::EmbeddedKeyProvider provider({{.id = readArray(source, "id", index), .mask = readArray(source, "mask", index), .sealed = readArray(source, "sealed", index)}});
        haylen::content::KeyProvider::KeyId keyId{};
        std::ranges::copy(id.getBytes(), keyId.begin());
        haylen::content::KeyProvider::Key opened{};
        ASSERT_TRUE(provider.getKey(keyId, opened));
        const std::vector<std::uint8_t> key = test::TestFiles::read(directory.getPath() / "keys" / ("content-" + id.toHex() + ".key"));
        EXPECT_TRUE(std::ranges::equal(opened, key));
        EXPECT_EQ(source.find(writeLiterals(key)), std::string::npos);
        EXPECT_EQ(source.find(haylen::content::Digest(std::span(key).first<32>()).toHex()), std::string::npos);
    }

    const std::vector<std::uint8_t> seed = test::TestFiles::read(directory.getPath() / "keys" / haylen::content::KeyStore::kSigningKeyFile);
    EXPECT_EQ(source.find(writeLiterals(seed)), std::string::npos);
    EXPECT_EQ(source.find(haylen::content::Digest(std::span(seed).first<32>()).toHex()), std::string::npos);
}

TEST_F(BootstrapWriterTest, EscapesTheTextItWrites) {
    const haylen::content::KeyStore keys = haylen::content::KeyStore::create(directory.getPath() / "keys", "dev.haylen.\"quoted\\");
    const std::string source = haylen::content::BootstrapWriter::write(keys, "apple", 1, "lua-5.5-f0-i4-x4-l8-n8-le");
    EXPECT_NE(source.find(".identifier = \"dev.haylen.\\042quoted\\134\""), std::string::npos) << source;
}

} // namespace haylen::lua::content
