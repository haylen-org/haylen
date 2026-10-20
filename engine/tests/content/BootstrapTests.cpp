#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "content/ReleasePackage.hpp"
#include "haylen/content/Bootstrap.hpp"
#include "haylen/content/EmbeddedKeyProvider.hpp"
#include "haylen/content/Error.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "support/ReleaseFixture.hpp"
#include "support/ServiceKeyProvider.hpp"
#include "support/TestFiles.hpp"

namespace haylen::content {

class BootstrapTest : public ::testing::Test {
  protected:
    BootstrapTest() {
        release.build({{"app.json", R"({"name": "Bootstrap", "identifier": "com.haylen.tests"})"}, {"source/main.lua", "loaded = true"}, {"content/data/config.json", R"({"level": 3})"}});
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

// A release build never plays plain files that someone put in the place of its release.
TEST_F(BootstrapTest, RefusesPlainFilesInThePlaceOfItsRelease) {
    const auto plain = std::make_shared<io::MemoryPackage>("plain", std::map<std::string, std::vector<std::uint8_t>>{{"app.json", test::TestFiles::bytes("{}")}, {"source/main.lua", test::TestFiles::bytes("print('swapped')")}});
    const Bootstrap bootstrap = makeBootstrap();
    expectCode(Error::Code::MissingManifest, [&] { (void)ReleasePackage::openBundled(plain, &bootstrap); });
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
    otherApp.identifier = "com.haylen.other";
    expectCode(Error::Code::ManifestIncompatible, [&] { (void)ReleasePackage::open(release.getFiles(), otherApp); });
}

TEST_F(BootstrapTest, InstallsTheBootstrapOfTheRunningApp) {
    EXPECT_TRUE(Bootstrap::install(makeBootstrap()));
    ASSERT_NE(Bootstrap::find(), nullptr);
    EXPECT_EQ(Bootstrap::find()->identifier, test::ReleaseFixture::kIdentifier);
    EXPECT_EQ(Bootstrap::find()->keys->getKeyIds(), std::vector{getKeyId()});
}

// A C++ app adds the provider of its service after the keys that its release build compiles in, and a key that the release lacks is asked of each provider in turn.
TEST_F(BootstrapTest, AddsAProviderAfterTheKeysOfTheApp) {
    EXPECT_TRUE(Bootstrap::install(makeBootstrap()));
    const auto service = std::make_shared<test::ServiceKeyProvider>();
    const KeyProvider::Key later = test::ReleaseFixture::makeKey(2);
    service->allow(later);
    Bootstrap::addKeyProvider(service);

    const KeyProvider& keys = *Bootstrap::find()->keys;
    const KeyProvider::KeyId laterId = test::ServiceKeyProvider::identify(later);
    // clang-format off
    const auto request = [&keys](const KeyProvider::KeyId& id) {
        std::promise<bool> answer;
        keys.requestKey(id, [&answer](bool given) { answer.set_value(given); });
        return answer.get_future().get();
    };
    // clang-format on
    EXPECT_EQ(keys.getKeyIds(), std::vector{getKeyId()});
    EXPECT_TRUE(request(laterId));
    EXPECT_EQ(keys.getKeyIds(), (std::vector{getKeyId(), laterId}));
    KeyProvider::Key given{};
    ASSERT_TRUE(keys.getKey(laterId, given));
    EXPECT_EQ(given, later);

    EXPECT_FALSE(request(KeyProvider::KeyId{}));
    EXPECT_EQ(service->getRequests(), 2);

    // The bootstrap of the process lets the service go, so the threads of its answers end with the test.
    EXPECT_TRUE(Bootstrap::install(makeBootstrap()));
}

} // namespace haylen::content
