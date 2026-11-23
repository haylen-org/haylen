#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "content/Compatibility.hpp"
#include "content/crypto/KeyRing.hpp"
#include "content/crypto/SigningKey.hpp"
#include "content/format/CatalogWriter.hpp"
#include "content/format/ChannelDescriptor.hpp"
#include "content/format/ChannelWriter.hpp"
#include "content/format/Manifest.hpp"
#include "content/format/ManifestWriter.hpp"
#include "haylen/content/Error.hpp"
#include "support/ReleaseFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::lua::content {

class ManifestTest : public ::testing::Test {
  protected:
    // The offsets of fields of the manifest format, where the string of the profile starts with its length.
    static constexpr std::size_t kEnvelopeSizeOffset = 12;
    static constexpr std::size_t kDomainOffset = 16;
    static constexpr std::size_t kShardVersionOffset = 18;
    static constexpr std::size_t kProfileOffset = 176;

    // The format of the Lua bytecode of a build, as the bootstrap and the manifests name it.
    static constexpr std::string_view kLuaAbi = "lua-5.5-f0-i4-x4-l8-n8-le";

    ManifestTest() : signingKey(test::ReleaseFixture::makeKey(101)), trusted{signingKey.getVerifyingKey()} {
        keyId = keys.add(test::ReleaseFixture::makeKey(1));
    }

    [[nodiscard]] static std::vector<std::uint8_t> makeCatalog(const std::string& path, haylen::content::Delivery delivery = haylen::content::Delivery::Required, haylen::content::Catalog::Kind kind = haylen::content::Catalog::Kind::File) {
        const haylen::content::Catalog::Chunk chunk{.storedId = haylen::content::Digest::of(test::TestFiles::bytes("stored")), .contentId = haylen::content::Digest::of(test::TestFiles::bytes("plain")), .plainSize = 10, .encodedSize = 10};
        haylen::content::CatalogWriter writer;
        writer.addChunk(chunk);
        writer.addFile(path, delivery, std::vector{chunk.storedId}, kind);
        return writer.write();
    }

    [[nodiscard]] static haylen::content::Manifest::Envelope makeEnvelope(haylen::content::Manifest::Domain domain = haylen::content::Manifest::Domain::Content) {
        return {
            .domain = domain,
            .application = haylen::content::Manifest::identifyApplication("com.haylen.tests"),
            .profile = "desktop",
            .channel = "stable",
            .generation = 3,
            .previousManifest = haylen::content::Digest::of(test::TestFiles::bytes("previous")),
            .minimumAppBuild = 5,
            .maximumAppBuild = 9,
            .shards = {{.shardId = haylen::content::Digest::of(test::TestFiles::bytes("shard")), .fileSize = 4096, .fileDigest = haylen::content::Digest::of(test::TestFiles::bytes("file"))}},
        };
    }

    [[nodiscard]] std::vector<std::uint8_t> write(const haylen::content::Manifest::Envelope& envelope, const std::vector<std::uint8_t>& catalog) const {
        return haylen::content::ManifestWriter::write(envelope, catalog, *keys.get(keyId), signingKey);
    }

    [[nodiscard]] std::vector<std::uint8_t> write() const {
        return write(makeEnvelope(), makeCatalog("content/maps/island.tmj"));
    }

    static void expectCode(haylen::content::Error::Code code, const std::function<void()>& action) {
        try {
            action();
            ADD_FAILURE() << "The action must fail.";
        } catch (const haylen::content::Error& error) {
            EXPECT_EQ(error.getCode(), code) << error.what();
        }
    }

    static void writeNumber(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint64_t value, std::size_t size) {
        for (std::size_t index = 0; index < size; ++index) {
            bytes[offset + index] = static_cast<std::uint8_t>(value >> (8 * index));
        }
    }

    // Changes the envelope of a valid manifest and signs it again with the trusted key, so only the rules of the envelope itself can reject it.
    void expectEnvelopeRejected(haylen::content::Error::Code code, const std::string& message, const std::function<void(std::vector<std::uint8_t>&)>& change) const {
        const std::vector<std::uint8_t> manifest = write();
        std::size_t size = 0;
        for (std::size_t index = 0; index < sizeof(std::uint32_t); ++index) {
            size |= std::size_t{manifest[kEnvelopeSizeOffset + index]} << (8 * index);
        }

        std::vector<std::uint8_t> changed(manifest.begin(), manifest.begin() + static_cast<std::ptrdiff_t>(size));
        change(changed);
        writeNumber(changed, kEnvelopeSizeOffset, changed.size(), sizeof(std::uint32_t));

        const haylen::content::VerifyingKey::Signature signature = signingKey.sign(changed);
        changed.insert(changed.end(), signature.begin(), signature.end());
        changed.insert(changed.end(), manifest.begin() + static_cast<std::ptrdiff_t>(size + haylen::content::VerifyingKey::kSignatureSize), manifest.end());
        try {
            (void)haylen::content::Manifest::read(std::move(changed), trusted);
            ADD_FAILURE() << "The envelope must be rejected with \"" << message << "\"";
        } catch (const haylen::content::Error& error) {
            EXPECT_EQ(error.getCode(), code);
            EXPECT_EQ(std::string(error.what()), message);
        }
    }

    haylen::content::KeyRing keys;
    haylen::content::Digest keyId;
    haylen::content::SigningKey signingKey;
    std::vector<haylen::content::VerifyingKey> trusted;
};

TEST_F(ManifestTest, ReadsBackWhatItSigned) {
    const std::vector<std::uint8_t> bytes = write();
    EXPECT_EQ(write(), bytes) << "The same release always signs to the same manifest.";

    const haylen::content::Manifest manifest = haylen::content::Manifest::read(bytes, trusted);
    const haylen::content::Manifest::Envelope& envelope = manifest.getEnvelope();
    const haylen::content::Manifest::Envelope expected = makeEnvelope();
    EXPECT_EQ(envelope.domain, haylen::content::Manifest::Domain::Content);
    EXPECT_EQ(envelope.application, expected.application);
    EXPECT_EQ(envelope.profile, "desktop");
    EXPECT_EQ(envelope.channel, "stable");
    EXPECT_EQ(envelope.generation, 3U);
    EXPECT_EQ(envelope.previousManifest, expected.previousManifest);
    EXPECT_EQ(envelope.minimumAppBuild, 5U);
    EXPECT_EQ(envelope.maximumAppBuild, 9U);
    EXPECT_EQ(envelope.contentKeyId, keyId);
    EXPECT_EQ(envelope.signingKeyId, signingKey.getVerifyingKey().getId());
    EXPECT_EQ(envelope.shards, expected.shards);
    EXPECT_EQ(haylen::content::Manifest::read(bytes, trusted).getId(), manifest.getId());

    const haylen::content::Catalog catalog = manifest.decryptCatalog(keys);
    EXPECT_TRUE(catalog.findFile("content/maps/island.tmj").has_value());

    const std::string text(bytes.begin(), bytes.end());
    EXPECT_EQ(text.find("island"), std::string::npos) << "No path appears in the bytes of a manifest.";
}

TEST_F(ManifestTest, RejectsUntrustedAndBrokenSignatures) {
    const std::vector<std::uint8_t> bytes = write();
    const haylen::content::SigningKey other(test::ReleaseFixture::makeKey(102));
    expectCode(haylen::content::Error::Code::ManifestSignatureInvalid, [&] { (void)haylen::content::Manifest::read(bytes, std::vector{other.getVerifyingKey()}); });

    // Every byte before the catalog is signed, so changing any of them makes the manifest unreadable.
    const std::size_t catalogSize = makeCatalog("content/maps/island.tmj").size();
    for (std::size_t index = 0; index < bytes.size() - catalogSize; ++index) {
        std::vector<std::uint8_t> changed = bytes;
        changed[index] ^= 0x04;
        try {
            (void)haylen::content::Manifest::read(std::move(changed), trusted);
            ADD_FAILURE() << "A change at byte " << index << " went unnoticed.";
        } catch (const haylen::content::Error&) {}
    }

    for (std::size_t size = 0; size < bytes.size(); size += 11) {
        try {
            (void)haylen::content::Manifest::read(std::vector<std::uint8_t>(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(size)), trusted);
            ADD_FAILURE() << "A manifest cut at " << size << " bytes was read.";
        } catch (const haylen::content::Error&) {}
    }
}

TEST_F(ManifestTest, RejectsChangedCatalogs) {
    std::vector<std::uint8_t> changed = write();
    changed.back() ^= 0x01;
    const haylen::content::Manifest manifest = haylen::content::Manifest::read(changed, trusted);
    expectCode(haylen::content::Error::Code::CatalogAuthenticationFailed, [&] { (void)manifest.decryptCatalog(keys); });

    std::vector<std::uint8_t> longer = write();
    longer.push_back(0);
    expectCode(haylen::content::Error::Code::CorruptManifest, [&] { (void)haylen::content::Manifest::read(longer, trusted); });

    haylen::content::KeyRing other;
    other.add(test::ReleaseFixture::makeKey(2));
    expectCode(haylen::content::Error::Code::UnknownKeyId, [&] { (void)haylen::content::Manifest::read(write(), trusted).decryptCatalog(other); });
}

TEST_F(ManifestTest, KeepsEachDomainToItsOwnFiles) {
    const auto decrypt = [this](haylen::content::Manifest::Domain domain, const std::string& path, haylen::content::Delivery delivery) { return haylen::content::Manifest::read(write(makeEnvelope(domain), makeCatalog(path, delivery)), trusted).decryptCatalog(keys).getFileCount(); };
    EXPECT_EQ(decrypt(haylen::content::Manifest::Domain::App, "haylen.json", haylen::content::Delivery::Required), 1U);
    EXPECT_EQ(decrypt(haylen::content::Manifest::Domain::App, "source/main.lua", haylen::content::Delivery::Required), 1U);
    EXPECT_EQ(decrypt(haylen::content::Manifest::Domain::App, "plugins/ads/source/init.lua", haylen::content::Delivery::Required), 1U);
    EXPECT_EQ(decrypt(haylen::content::Manifest::Domain::Content, "content/a.png", haylen::content::Delivery::OnDemand), 1U);
    expectCode(haylen::content::Error::Code::CorruptCatalog, [&] { (void)decrypt(haylen::content::Manifest::Domain::App, "content/a.png", haylen::content::Delivery::Required); });
    expectCode(haylen::content::Error::Code::CorruptCatalog, [&] { (void)decrypt(haylen::content::Manifest::Domain::App, "source/level.lua", haylen::content::Delivery::OnDemand); });
    expectCode(haylen::content::Error::Code::CorruptCatalog, [&] { (void)decrypt(haylen::content::Manifest::Domain::Content, "source/main.lua", haylen::content::Delivery::Required); });
    expectCode(haylen::content::Error::Code::CorruptCatalog, [&] { (void)decrypt(haylen::content::Manifest::Domain::Content, "haylen.json", haylen::content::Delivery::Required); });
}

TEST_F(ManifestTest, KeepsLuaBytecodeToTheModulesOfTheAppDomain) {
    haylen::content::Manifest::Envelope app = makeEnvelope(haylen::content::Manifest::Domain::App);
    app.luaAbi = kLuaAbi;
    const auto decrypt = [this](const haylen::content::Manifest::Envelope& envelope, const std::string& path) { return haylen::content::Manifest::read(write(envelope, makeCatalog(path, haylen::content::Delivery::Required, haylen::content::Catalog::Kind::LuaBytecode)), trusted).decryptCatalog(keys).getFileCount(); };
    EXPECT_EQ(decrypt(app, "source/main.lua"), 1U);
    EXPECT_EQ(decrypt(app, "source/scenes/menu.lua"), 1U);
    EXPECT_EQ(decrypt(app, "plugins/ads/source/init.lua"), 1U);
    EXPECT_EQ(haylen::content::Manifest::read(write(app, makeCatalog("source/main.lua")), trusted).getEnvelope().luaAbi, kLuaAbi);

    for (const char* path : {"haylen.json", "source/notes.txt", "plugins/ads/plugin.json", "plugins/ads/main.lua", "plugins/source/init.lua"}) {
        expectCode(haylen::content::Error::Code::CorruptCatalog, [&] { (void)decrypt(app, path); });
    }

    EXPECT_FALSE(haylen::content::Manifest::isLuaModule("content/script.lua")) << "Only the source folders of the app and its plugins hold modules.";

    haylen::content::Manifest::Envelope content = makeEnvelope(haylen::content::Manifest::Domain::Content);
    content.luaAbi = kLuaAbi;
    expectCode(haylen::content::Error::Code::CorruptCatalog, [&] { (void)decrypt(content, "content/script.lua"); });
    app.luaAbi.clear();
    expectCode(haylen::content::Error::Code::CorruptCatalog, [&] { (void)decrypt(app, "source/main.lua"); });
}

TEST_F(ManifestTest, ChecksTheCompatibilityOfTheApp) {
    const haylen::content::Manifest manifest = haylen::content::Manifest::read(write(), trusted);
    const haylen::content::Compatibility compatibility{.application = haylen::content::Manifest::identifyApplication("com.haylen.tests"), .appBuild = 5, .profile = "desktop", .luaAbi = std::string(kLuaAbi)};
    compatibility.check(manifest);
    haylen::content::Compatibility newest = compatibility;
    newest.appBuild = 9;
    newest.check(manifest);

    haylen::content::Compatibility otherApp = compatibility;
    otherApp.application = haylen::content::Manifest::identifyApplication("com.haylen.other");
    haylen::content::Compatibility otherProfile = compatibility;
    otherProfile.profile = "web";
    haylen::content::Compatibility older = compatibility;
    older.appBuild = 4;
    haylen::content::Compatibility newer = compatibility;
    newer.appBuild = 10;

    for (const haylen::content::Compatibility& other : {otherApp, otherProfile, older, newer}) {
        expectCode(haylen::content::Error::Code::ManifestIncompatible, [&] { other.check(manifest); });
    }

    // Bytecode loads only into a build whose Lua writes the same binary chunks.
    haylen::content::Manifest::Envelope app = makeEnvelope(haylen::content::Manifest::Domain::App);
    app.luaAbi = "lua-5.4-f0-i4-x4-l8-n8-le";
    const haylen::content::Manifest older54 = haylen::content::Manifest::read(write(app, makeCatalog("source/main.lua", haylen::content::Delivery::Required, haylen::content::Catalog::Kind::LuaBytecode)), trusted);
    expectCode(haylen::content::Error::Code::LuaBytecodeIncompatible, [&] { compatibility.check(older54); });
    app.luaAbi = kLuaAbi;
    compatibility.check(haylen::content::Manifest::read(write(app, makeCatalog("source/main.lua", haylen::content::Delivery::Required, haylen::content::Catalog::Kind::LuaBytecode)), trusted));
}

TEST_F(ManifestTest, RejectsInvalidEnvelopes) {
    haylen::content::Manifest::Envelope envelope = makeEnvelope();
    envelope.profile = "Desktop";
    EXPECT_THROW((void)write(envelope, makeCatalog("content/a.png")), std::invalid_argument);
    envelope = makeEnvelope();
    envelope.profile.clear();
    EXPECT_THROW((void)write(envelope, makeCatalog("content/a.png")), std::invalid_argument);
    envelope = makeEnvelope();
    envelope.minimumAppBuild = 10;
    EXPECT_THROW((void)write(envelope, makeCatalog("content/a.png")), std::invalid_argument);
    envelope = makeEnvelope();
    envelope.luaAbi = "Lua 5.5";
    EXPECT_THROW((void)write(envelope, makeCatalog("content/a.png")), std::invalid_argument);
}

TEST_F(ManifestTest, KeepsTheRulesOfASignedEnvelope) {
    // A trusted signature tells who wrote an envelope, never that its fields are valid. The test envelope ends with the count of its shards, its one shard of 72 bytes and the catalog fields of 80 bytes, which start with the size of the catalog.
    const auto catalogSizeOffset = [](const std::vector<std::uint8_t>& envelope) { return envelope.size() - 80; };
    const auto shardCountOffset = [&catalogSizeOffset](const std::vector<std::uint8_t>& envelope) { return catalogSizeOffset(envelope) - 72 - 8; };
    expectEnvelopeRejected(haylen::content::Error::Code::CorruptManifest, "The manifest envelope names a domain this app does not know.", [](std::vector<std::uint8_t>& envelope) { envelope[kDomainOffset] = 2; });
    expectEnvelopeRejected(haylen::content::Error::Code::UnsupportedVersion, "The manifest uses shard or catalog versions this app does not read.", [](std::vector<std::uint8_t>& envelope) { envelope[kShardVersionOffset] = 2; });
    expectEnvelopeRejected(haylen::content::Error::Code::CorruptManifest, "The manifest envelope holds a string longer than its format allows.", [](std::vector<std::uint8_t>& envelope) { envelope[kProfileOffset] = static_cast<std::uint8_t>(haylen::content::Manifest::kMaximumNameLength + 1); });
    expectEnvelopeRejected(haylen::content::Error::Code::CorruptManifest, "The manifest envelope holds a profile, a channel, a Lua ABI or a build range that is not valid.", [](std::vector<std::uint8_t>& envelope) { envelope[kProfileOffset + 2] = 'D'; });
    expectEnvelopeRejected(haylen::content::Error::Code::CorruptManifest, "The manifest envelope lists more shards than it holds.", [&](std::vector<std::uint8_t>& envelope) { writeNumber(envelope, shardCountOffset(envelope), 3, sizeof(std::uint64_t)); });
    expectEnvelopeRejected(haylen::content::Error::Code::CorruptManifest, "The manifest envelope ends before its fields do.", [&](std::vector<std::uint8_t>& envelope) { writeNumber(envelope, shardCountOffset(envelope), 2, sizeof(std::uint64_t)); });
    expectEnvelopeRejected(haylen::content::Error::Code::CorruptManifest, "The manifest envelope has bytes after its last field.", [](std::vector<std::uint8_t>& envelope) { envelope.push_back(0); });
    expectEnvelopeRejected(haylen::content::Error::Code::CorruptManifest, "The manifest envelope declares a catalog larger than the limit.", [&](std::vector<std::uint8_t>& envelope) { writeNumber(envelope, catalogSizeOffset(envelope), haylen::content::Manifest::kMaximumCatalogSize + 1, sizeof(std::uint64_t)); });
}

TEST_F(ManifestTest, NeverAcceptsAnOlderGeneration) {
    const haylen::content::Manifest manifest = haylen::content::Manifest::read(write(), trusted);
    const haylen::content::ChannelDescriptor descriptor{.application = manifest.getEnvelope().application, .channel = "stable", .generation = 3, .manifest = manifest.getId()};
    const haylen::content::ChannelDescriptor read = haylen::content::ChannelDescriptor::read(haylen::content::ChannelWriter::write(descriptor, signingKey), trusted);
    EXPECT_EQ(read.generation, 3U);
    EXPECT_EQ(read.manifest, manifest.getId());
    EXPECT_EQ(read.signingKeyId, signingKey.getVerifyingKey().getId());
    EXPECT_TRUE(read.describes(manifest));

    EXPECT_TRUE(read.offersUpdate(2, haylen::content::Digest::of(test::TestFiles::bytes("older"))));
    EXPECT_FALSE(read.offersUpdate(3, manifest.getId()));
    expectCode(haylen::content::Error::Code::ManifestRollbackRejected, [&] { (void)read.offersUpdate(4, haylen::content::Digest::of(test::TestFiles::bytes("newer"))); });
    expectCode(haylen::content::Error::Code::ManifestRollbackRejected, [&] { (void)read.offersUpdate(3, haylen::content::Digest::of(test::TestFiles::bytes("same generation"))); });

    haylen::content::ChannelDescriptor other = read;
    other.generation = 4;
    EXPECT_FALSE(other.describes(manifest));
    EXPECT_FALSE(read.describes(haylen::content::Manifest::read(write(makeEnvelope(haylen::content::Manifest::Domain::App), makeCatalog("haylen.json")), trusted)));
}

TEST_F(ManifestTest, VerifiesChannelDescriptors) {
    const haylen::content::ChannelDescriptor descriptor{.application = haylen::content::Manifest::identifyApplication("com.haylen.tests"), .channel = "beta", .generation = 12, .manifest = haylen::content::Digest::of(test::TestFiles::bytes("manifest"))};
    const std::vector<std::uint8_t> bytes = haylen::content::ChannelWriter::write(descriptor, signingKey);
    const haylen::content::SigningKey other(test::ReleaseFixture::makeKey(102));
    expectCode(haylen::content::Error::Code::ManifestSignatureInvalid, [&] { (void)haylen::content::ChannelDescriptor::read(bytes, std::vector{other.getVerifyingKey()}); });

    for (std::size_t index = 0; index < bytes.size(); ++index) {
        std::vector<std::uint8_t> changed = bytes;
        changed[index] ^= 0x01;
        try {
            (void)haylen::content::ChannelDescriptor::read(changed, trusted);
            ADD_FAILURE() << "A change at byte " << index << " went unnoticed.";
        } catch (const haylen::content::Error&) {}
    }
}

} // namespace haylen::lua::content
