#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "content/BootstrapWriter.hpp"
#include "content/KeyStore.hpp"
#include "content/crypto/Digest.hpp"
#include "haylen/content/EmbeddedKeyProvider.hpp"
#include "support/TemporaryDirectory.hpp"

namespace haylen::content {

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

    [[nodiscard]] std::vector<std::uint8_t> readFile(const std::string& name) const {
        std::ifstream stream(directory.getPath() / "keys" / name, std::ios::binary);
        return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
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
    KeyStore keys = KeyStore::create(directory.getPath() / "keys", "dev.haylen.tests");
    keys.rotate();
    const std::string source = BootstrapWriter::write(keys, "apple", 1002003);
    EXPECT_EQ(BootstrapWriter::write(keys, "apple", 1002003), source) << "The same keys always write the same bootstrap.";
    EXPECT_NE(source.find(".identifier = \"dev.haylen.tests\", .appBuild = 1002003U, .profile = \"apple\""), std::string::npos) << source;
    EXPECT_NE(source.find("const bool HaylenBootstrap::kInstalled = HaylenBootstrap::install();"), std::string::npos);

    // Each sealed key opens to the key of its file, while the source holds neither a key nor the seed of the signing key in any form.
    for (std::size_t index = 0; index < keys.getContentKeyIds().size(); ++index) {
        const Digest& id = keys.getContentKeyIds()[index];
        const EmbeddedKeyProvider provider({{.id = readArray(source, "id", index), .mask = readArray(source, "mask", index), .sealed = readArray(source, "sealed", index)}});
        KeyProvider::KeyId keyId{};
        std::ranges::copy(id.getBytes(), keyId.begin());
        KeyProvider::Key opened{};
        ASSERT_TRUE(provider.getKey(keyId, opened));
        const std::vector<std::uint8_t> key = readFile("content-" + id.toHex() + ".key");
        EXPECT_TRUE(std::ranges::equal(opened, key));
        EXPECT_EQ(source.find(writeLiterals(key)), std::string::npos);
        EXPECT_EQ(source.find(Digest(std::span(key).first<32>()).toHex()), std::string::npos);
    }
    const std::vector<std::uint8_t> seed = readFile(std::string(KeyStore::kSigningKeyFile));
    EXPECT_EQ(source.find(writeLiterals(seed)), std::string::npos);
    EXPECT_EQ(source.find(Digest(std::span(seed).first<32>()).toHex()), std::string::npos);
}

TEST_F(BootstrapWriterTest, EscapesTheTextItWrites) {
    const KeyStore keys = KeyStore::create(directory.getPath() / "keys", "dev.haylen.\"quoted\\");
    const std::string source = BootstrapWriter::write(keys, "apple", 1);
    EXPECT_NE(source.find(".identifier = \"dev.haylen.\\042quoted\\134\""), std::string::npos) << source;
}

} // namespace haylen::content
