#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "content/Error.hpp"
#include "content/ReleasePackage.hpp"
#include "haylen/content/Bootstrap.hpp"
#include "haylen/content/EmbeddedKeyProvider.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "support/ReleaseFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::content {

class BootstrapTest : public ::testing::Test {
  protected:
    BootstrapTest() {
        release.build({{"app.json", R"({"name": "Bootstrap", "identifier": "dev.haylen.tests"})"}, {"source/main.lua", "loaded = true"}, {"content/data/config.json", R"({"level": 3})"}});
    }

    // The bootstrap that a release build of the test app compiles in.
    [[nodiscard]] Bootstrap makeBootstrap() const {
        Bootstrap bootstrap{.identifier = std::string(test::ReleaseFixture::kIdentifier), .appBuild = test::ReleaseFixture::kAppBuild, .profile = std::string(test::ReleaseFixture::kProfile)};
        Bootstrap::PublicKey trusted{};
        std::ranges::copy(release.getSigningKey().getVerifyingKey().getBytes(), trusted.begin());
        bootstrap.trustedKeys.push_back(trusted);
        bootstrap.keys = std::make_shared<const EmbeddedKeyProvider>(std::vector{EmbeddedKeyProvider::seal(getKeyId(), test::ReleaseFixture::makeKey(1))});
        return bootstrap;
    }

    [[nodiscard]] KeyProvider::KeyId getKeyId() const {
        KeyProvider::KeyId id{};
        std::ranges::copy(release.getContentKey()->getId().getBytes(), id.begin());
        return id;
    }

    static void expectCode(Error::Code code, const std::function<void()>& action) {
        try {
            action();
            ADD_FAILURE() << "The action must fail.";
        } catch (const Error& error) {
            EXPECT_EQ(error.getCode(), code) << error.what();
            EXPECT_EQ(std::string(error.what()).find(Digest(test::ReleaseFixture::makeKey(1)).toHex()), std::string::npos) << "No message names a key.";
        }
    }

    test::ReleaseFixture release;
};

TEST_F(BootstrapTest, SealsKeysThatOnlyTheProviderOpens) {
    const KeyProvider::Key key = test::ReleaseFixture::makeKey(1);
    const EmbeddedKeyProvider::SealedKey sealed = EmbeddedKeyProvider::seal(getKeyId(), key);
    EXPECT_NE(sealed.sealed, key);
    EXPECT_NE(sealed.mask, key);
    EXPECT_EQ(EmbeddedKeyProvider::seal(getKeyId(), key).sealed, sealed.sealed) << "The same key always seals to the same constants.";

    const EmbeddedKeyProvider provider({sealed});
    EXPECT_EQ(provider.getKeyIds(), std::vector{getKeyId()});
    KeyProvider::Key opened{};
    ASSERT_TRUE(provider.getKey(getKeyId(), opened));
    EXPECT_EQ(opened, key);
    EXPECT_FALSE(provider.getKey(KeyProvider::KeyId{}, opened));
}

TEST_F(BootstrapTest, OpensTheReleaseOfItsApp) {
    const std::unique_ptr<io::Package> package = ReleasePackage::open(release.getFiles(), makeBootstrap());
    EXPECT_EQ(package->readAssetText("data/config.json"), R"({"level": 3})");
    EXPECT_TRUE(package->isLuaBytecode("source/main.lua"));

    // A development build ships the package itself, which opens as it is with or without a bootstrap.
    const auto plain = std::make_shared<io::MemoryPackage>("plain", std::map<std::string, std::vector<std::uint8_t>>{{"app.json", test::TestFiles::bytes("{}")}});
    EXPECT_EQ(ReleasePackage::openBundled(plain, nullptr), plain);
    const Bootstrap bootstrap = makeBootstrap();
    const std::shared_ptr<io::Package> folder = io::Package::openDirectory(release.getFolder());
    EXPECT_TRUE(ReleasePackage::openBundled(folder, &bootstrap)->isLuaBytecode("source/main.lua"));
}

TEST_F(BootstrapTest, RefusesReleasesItHasNoKeysFor) {
    expectCode(Error::Code::KeyUnavailable, [&] { (void)ReleasePackage::openBundled(io::Package::openDirectory(release.getFolder()), nullptr); });

    Bootstrap damaged = makeBootstrap();
    EmbeddedKeyProvider::SealedKey sealed = EmbeddedKeyProvider::seal(getKeyId(), test::ReleaseFixture::makeKey(1));
    sealed.sealed[0] ^= 1;
    damaged.keys = std::make_shared<const EmbeddedKeyProvider>(std::vector{sealed});
    expectCode(Error::Code::KeyUnavailable, [&] { (void)ReleasePackage::open(release.getFiles(), damaged); });

    Bootstrap other = makeBootstrap();
    const KeyProvider::Key otherKey = test::ReleaseFixture::makeKey(2);
    KeyProvider::KeyId otherId{};
    std::ranges::copy(ContentKey(otherKey).getId().getBytes(), otherId.begin());
    other.keys = std::make_shared<const EmbeddedKeyProvider>(std::vector{EmbeddedKeyProvider::seal(otherId, otherKey)});
    expectCode(Error::Code::UnknownKeyId, [&] { (void)ReleasePackage::open(release.getFiles(), other); });

    Bootstrap otherApp = makeBootstrap();
    otherApp.identifier = "dev.haylen.other";
    expectCode(Error::Code::ManifestIncompatible, [&] { (void)ReleasePackage::open(release.getFiles(), otherApp); });
}

TEST_F(BootstrapTest, InstallsTheBootstrapOfTheRunningApp) {
    EXPECT_TRUE(Bootstrap::install(makeBootstrap()));
    ASSERT_NE(Bootstrap::find(), nullptr);
    EXPECT_EQ(Bootstrap::find()->identifier, test::ReleaseFixture::kIdentifier);
    EXPECT_EQ(Bootstrap::find()->keys->getKeyIds(), std::vector{getKeyId()});
}

} // namespace haylen::content
