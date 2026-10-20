#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "content/ChannelPublisher.hpp"
#include "content/Downloader.hpp"
#include "content/FolderStoreFiles.hpp"
#include "content/HttpTransport.hpp"
#include "content/PublicationLayout.hpp"
#include "content/ReleasePackage.hpp"
#include "content/Updater.hpp"
#include "content/format/ShardHeader.hpp"
#include "haylen/content/Error.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "support/BrowserStoreFiles.hpp"
#include "support/ContentServer.hpp"
#include "support/CrashingStoreFiles.hpp"
#include "support/LimitedStoreFiles.hpp"
#include "support/QuotaStoreFiles.hpp"
#include "support/ReleaseFixture.hpp"
#include "support/ServiceKeyProvider.hpp"
#include "support/TemporaryDirectory.hpp"
#include "support/TestFiles.hpp"
#include "support/VarnRuntime.hpp"

namespace haylen::content {

// The update of the content of an app from the channel of a publication tree that a local server serves, as an installed app goes through it: checks, downloads that stop and resume, the switch to the new content at the next start, and the fallbacks when a download or the content on the device is damaged.
class UpdaterTest : public ::testing::Test {
  protected:
    using Files = std::map<std::string, std::vector<std::uint8_t>>;

    static constexpr std::size_t kMegabyte = 1024 * 1024;

    // An installed app: its release with the local store of its downloads and the parts that update it.
    struct Client {
        Client(UpdaterTest& test, std::unique_ptr<StoreFiles> storeFiles, std::shared_ptr<const KeyRing> keys = nullptr, std::shared_ptr<const KeyProvider> provider = nullptr) : jobs(test.jobSystems.emplace_back(test.runtime.getRuntime(), [](const std::string& error) { ADD_FAILURE() << error; })), transport(test.runtime.getRuntime()) {
            const VerifyingKey trusted = test.release.getSigningKey().getVerifyingKey();
            auto recordStore = std::make_shared<LocalStore>(std::move(storeFiles), test.release.getCompatibility().application, test::ReleaseFixture::kAppBuild);
            package = std::make_shared<ReleasePackage>(test.release.getFiles(), keys ? std::move(keys) : test.release.getKeys(), std::span(&trusted, 1), test.release.getCompatibility(), std::move(recordStore), std::move(provider));
            downloader = std::make_unique<Downloader>(jobs, transport, package->getStore());
            updater = std::make_unique<Updater>(package, jobs, transport, *downloader, core::AppConfig::Content{.updates = core::AppConfig::Content::Updates{.baseUrl = test.server.getUrl()}});
        }

        std::shared_ptr<ReleasePackage> package;
        core::JobSystem& jobs;
        HttpTransport transport;
        std::unique_ptr<Downloader> downloader;
        std::unique_ptr<Updater> updater;
    };

    UpdaterTest() {
        game = makeGame();
        release.build(io::MemoryPackage("source", game));
    }

    [[nodiscard]] static Files makeGame() {
        Files files;
        files.emplace("app.json", test::TestFiles::bytes(R"({"name": "Updates", "identifier": "com.haylen.tests"})"));
        files.emplace("source/main.lua", test::TestFiles::bytes("return 1"));
        files.emplace("content/data/config.json", test::TestFiles::bytes(R"({"level": 1})"));
        files.emplace("content/music/theme.ogg", test::TestFiles::randomBytes(3 * kMegabyte, 11));
        files.emplace("content/images/hero.png", test::TestFiles::randomBytes(200 * 1024, 12));
        return files;
    }

    ChannelPublisher::Publication publish(const Files& files, std::uint64_t appBuild = test::ReleaseFixture::kAppBuild) const {
        return publish(files, release.getKeys(), release.getContentKeyId(), appBuild);
    }

    ChannelPublisher::Publication publish(const Files& files, std::shared_ptr<const KeyRing> keys, const Digest& keyId, std::uint64_t appBuild = test::ReleaseFixture::kAppBuild) const {
        const ChannelPublisher publisher(std::move(keys), keyId, release.getSharedSigningKey());
        return publisher.publish(io::MemoryPackage("source", files), tree.getPath(), {.profile = std::string(test::ReleaseFixture::kProfile), .appBuild = appBuild, .channel = "stable", .release = release.getFolder()});
    }

    [[nodiscard]] Files makeUpdate(int level) const {
        Files files = game;
        files["content/data/config.json"] = test::TestFiles::bytes(R"({"level": )" + std::to_string(level) + "}");
        files["content/maps/level.tmj"] = test::TestFiles::randomBytes(100 * 1024, static_cast<std::uint64_t>(level));
        return files;
    }

    [[nodiscard]] std::unique_ptr<Client> connect(std::shared_ptr<const KeyRing> keys = nullptr, std::shared_ptr<const KeyProvider> provider = nullptr) {
        return std::make_unique<Client>(*this, std::make_unique<FolderStoreFiles>(store.getPath()), std::move(keys), std::move(provider));
    }

    std::pair<std::optional<UpdateOffer>, std::optional<Error>> check(Client& client) {
        std::optional<std::pair<std::optional<UpdateOffer>, std::optional<Error>>> result;
        client.updater->check([&result](std::optional<UpdateOffer> offer, std::optional<Error> error) { result.emplace(std::move(offer), std::move(error)); });
        EXPECT_TRUE(runtime.pumpUntil([&result] { return result.has_value(); }));
        return std::move(*result);
    }

    Updater::Collection collect(Client& client) {
        std::optional<Updater::Collection> collection;
        // clang-format off
        client.updater->collect([&collection](Updater::Collection collected, std::optional<Error> error) {
            EXPECT_FALSE(error);
            collection = collected;
        });
        // clang-format on
        EXPECT_TRUE(runtime.pumpUntil([&collection] { return collection.has_value(); }));
        return collection.value_or(Updater::Collection{});
    }

    UpdateOffer requireOffer(Client& client) {
        auto [offer, error] = check(client);
        EXPECT_FALSE(error) << error->what();
        EXPECT_TRUE(offer);
        return std::move(*offer);
    }

    std::optional<Error> install(Client& client, const UpdateOffer& offer) {
        std::optional<std::optional<Error>> result;
        client.updater->install(offer, {}, [&result](std::optional<Error> error) { result.emplace(std::move(error)); });
        EXPECT_TRUE(runtime.pumpUntil([&result] { return result.has_value(); }, std::chrono::seconds(20)));
        return std::move(*result);
    }

    [[nodiscard]] static std::string readText(const io::Package& package, std::string_view path) {
        const std::vector<std::uint8_t> bytes = package.read(path);
        return {bytes.begin(), bytes.end()};
    }

    test::ReleaseFixture release;
    Files game;
    test::TemporaryDirectory tree;
    test::TemporaryDirectory store;
    test::ContentServer server{tree.getPath()};

    // The job systems of the clients outlive the pools, as the one of an engine does, so a job that a client left running when it went away still finds the job system it posts through.
    std::list<core::JobSystem> jobSystems;
    test::VarnRuntime runtime;
};

TEST_F(UpdaterTest, InstallsAnUpdateThatPlaysFromTheNextStart) {
    publish(makeUpdate(2));
    {
        const std::unique_ptr<Client> client = connect();
        const UpdateOffer offer = requireOffer(*client);
        EXPECT_EQ(offer.generation, 1U);
        server.clearExchanges();
        ASSERT_FALSE(install(*client, offer));

        // The content plays as it was until the app starts again.
        EXPECT_EQ(readText(*client->package, "content/data/config.json"), R"({"level": 1})");
        EXPECT_EQ(client->package->getStore()->getState().active, offer.content->manifest->getId());
    }

    const std::unique_ptr<Client> client = connect();
    EXPECT_EQ(readText(*client->package, "content/data/config.json"), R"({"level": 2})");
    EXPECT_EQ(client->package->read("content/maps/level.tmj"), makeUpdate(2).at("content/maps/level.tmj"));
    EXPECT_EQ(client->package->read("content/music/theme.ogg"), game.at("content/music/theme.ogg"));
    EXPECT_FALSE(client->package->takeRecovery());

    auto [offer, error] = check(*client);
    EXPECT_FALSE(offer);
    EXPECT_FALSE(error);
}

// An online game hands out the key of an update from its service, so the app plays the content it has until the service gives the key, and the update installs once it does.
TEST_F(UpdaterTest, AsksTheKeyProviderForTheKeyOfAnUpdate) {
    const KeyProvider::Key rotatedKey = test::ReleaseFixture::makeKey(2);
    auto rotated = std::make_shared<KeyRing>(*release.getKeys());
    const Digest rotatedId = rotated->add(rotatedKey);
    publish(makeUpdate(2), rotated, rotatedId);
    {
        const auto service = std::make_shared<test::ServiceKeyProvider>();
        const std::unique_ptr<Client> client = connect(release.getKeys(), service);
        auto [refused, error] = check(*client);
        EXPECT_FALSE(refused);
        ASSERT_TRUE(error);
        EXPECT_EQ(error->getCode(), Error::Code::KeyUnavailable) << error->what();
        EXPECT_EQ(error->getDetails().keyId, rotatedId.toHex());
        EXPECT_EQ(service->getRequests(), 1);
        EXPECT_EQ(readText(*client->package, "content/data/config.json"), R"({"level": 1})");

        service->allow(rotatedKey);
        const UpdateOffer offer = requireOffer(*client);
        EXPECT_EQ(service->getRequests(), 2);
        ASSERT_FALSE(install(*client, offer));
    }

    // A start at which the provider does not hold the key yet plays the content before and keeps the update for a later start.
    {
        const std::unique_ptr<Client> client = connect(release.getKeys(), std::make_shared<test::ServiceKeyProvider>());
        EXPECT_EQ(readText(*client->package, "content/data/config.json"), R"({"level": 1})");
        EXPECT_FALSE(client->package->takeRecovery());
        EXPECT_FALSE(client->package->getStore()->getState().active.isZero());
    }

    const std::unique_ptr<Client> client = connect(rotated);
    EXPECT_EQ(readText(*client->package, "content/data/config.json"), R"({"level": 2})");
}

// A connection that drops while the manifest of an update downloads leaves the content as it was, and the next check finds the update again.
TEST_F(UpdaterTest, KeepsTheContentWhenTheDownloadOfTheManifestStops) {
    publish(makeUpdate(2));
    const std::unique_ptr<Client> client = connect();
    server.dropAfter(256);
    auto [offer, error] = check(*client);
    EXPECT_FALSE(offer);
    EXPECT_TRUE(error);
    EXPECT_TRUE(client->package->getStore()->getState().pending.isZero());
    EXPECT_TRUE(client->package->getStore()->getState().active.isZero());

    const UpdateOffer again = requireOffer(*client);
    ASSERT_FALSE(install(*client, again));
}

// An app that stops while it checks for an update leaves the store as it was, and the next start finds the update again. The server holds its answers, so the app stops in the middle of its check however fast the machine is.
TEST_F(UpdaterTest, LeavesTheStoreAsItWasWhenTheAppStopsWhileItChecks) {
    publish(makeUpdate(2));
    {
        std::unique_ptr<Client> client = connect();
        server.setStalls(true);
        client->updater->check([](std::optional<UpdateOffer>, std::optional<Error>) { ADD_FAILURE() << "An updater that is gone never answers."; });
        ASSERT_TRUE(runtime.pumpUntil([this] { return server.getAnswering() > 0; }));
        client.reset();
        server.setStalls(false);
        (void)runtime.pumpUntil([] { return false; }, std::chrono::milliseconds(200));
    }

    const std::unique_ptr<Client> client = connect();
    EXPECT_TRUE(client->package->getStore()->getState().pending.isZero());
    EXPECT_TRUE(client->package->getStore()->getState().active.isZero());
    (void)requireOffer(*client);
}

// A loss of power while the new content becomes active tears the state that names it, so the next start plays the content before it and installs the update again from the records that reached the disk.
TEST_F(UpdaterTest, KeepsTheContentWhenThePowerFailsAsTheUpdateBecomesActive) {
    publish(makeUpdate(2));
    {
        auto files = std::make_unique<test::CrashingStoreFiles>(store.getPath());
        test::CrashingStoreFiles& crashing = *files;
        const auto client = std::make_unique<Client>(*this, std::move(files));
        const UpdateOffer offer = requireOffer(*client);

        // The install writes the state twice: once to mark the update pending and once to make it active.
        crashing.crashOnWrite("state-", 1);
        EXPECT_TRUE(install(*client, offer));
        EXPECT_TRUE(crashing.hasCrashed());
    }
    {
        const std::unique_ptr<Client> client = connect();
        EXPECT_EQ(readText(*client->package, "content/data/config.json"), R"({"level": 1})");
        EXPECT_TRUE(client->package->getStore()->getState().active.isZero());
        const UpdateOffer offer = requireOffer(*client);
        EXPECT_EQ(offer.downloadBytes, 0U) << "The records that reached the disk before the loss of power stay.";
        server.clearExchanges();
        ASSERT_FALSE(install(*client, offer));
        EXPECT_EQ(server.getSentBytes(), 0U);
    }

    const std::unique_ptr<Client> client = connect();
    EXPECT_EQ(readText(*client->package, "content/data/config.json"), R"({"level": 2})");
}

// A store of the web lets go of the records of a large download past its budget, so a download that a load waits for reads its records in again before it ends, while a download that only keeps content on the device, such as a prefetch, reads nothing in.
TEST_F(UpdaterTest, ReadsInTheRecordsOfADownloadThatALoadWaitsFor) {
    publish(makeUpdate(2));
    for (const Downloader::Use use : {Downloader::Use::Keep, Downloader::Use::Read}) {
        const test::TemporaryDirectory folder;
        auto files = std::make_unique<test::BrowserStoreFiles>(folder.getPath());
        test::BrowserStoreFiles& browser = *files;
        browser.dropAppended();
        const auto client = std::make_unique<Client>(*this, std::move(files));
        const UpdateOffer offer = requireOffer(*client);
        std::optional<std::optional<Error>> fetched;
        // clang-format off
        client->downloader->fetch(offer.content->shards, offer.missing, [&client](const ShardReference& pack) { return client->updater->locate(pack); }, {
            .onProgress = {},
            .onComplete = [&fetched](std::optional<Error> error) { fetched.emplace(std::move(error)); },
        }, Downloader::Priority::Foreground, use);
        // clang-format on
        ASSERT_TRUE(runtime.pumpUntil([&fetched] { return fetched.has_value(); }));
        ASSERT_FALSE(*fetched);

        const bool reads = use == Downloader::Use::Read;
        EXPECT_EQ(browser.getLoadedCount(), reads ? offer.missing.size() : 0U);
        std::vector<std::uint8_t> scratch;
        std::vector<std::uint8_t> plain;
        for (const Catalog::Chunk& chunk : offer.missing) {
            if (reads) {
                EXPECT_NO_THROW(offer.content->shards->readChunk(chunk, scratch, plain));
            } else {
                EXPECT_THROW(offer.content->shards->readChunk(chunk, scratch, plain), Error);
            }
        }

        // Keeping content that the store holds already reads nothing in and downloads nothing.
        std::optional<std::optional<Error>> kept;
        server.clearExchanges();
        client->downloader->fetch(offer.content->shards, offer.missing, [&client](const ShardReference& pack) { return client->updater->locate(pack); }, {.onComplete = [&kept](std::optional<Error> error) { kept.emplace(std::move(error)); }});
        ASSERT_TRUE(runtime.pumpUntil([&kept] { return kept.has_value(); }));
        EXPECT_EQ(server.getSentBytes(), 0U);
        EXPECT_EQ(browser.getLoadedCount(), reads ? offer.missing.size() : 0U);
    }
}

// A store of the web writes through a worker that may fall behind the network, so downloads hold their next transfers while it holds too much that it has not written, and go on once it wrote it.
TEST_F(UpdaterTest, HoldsTransfersWhileTheStoreFallsBehind) {
    publish(makeUpdate(2));
    auto files = std::make_unique<test::BrowserStoreFiles>(store.getPath());
    test::BrowserStoreFiles& browser = *files;
    const auto client = std::make_unique<Client>(*this, std::move(files));
    const UpdateOffer offer = requireOffer(*client);
    browser.setUnwrittenBytes(Downloader::kMaximumHeldBytes + 1);
    server.clearExchanges();

    std::optional<std::optional<Error>> fetched;
    // clang-format off
    client->downloader->fetch(offer.content->shards, offer.missing, [&client](const ShardReference& pack) { return client->updater->locate(pack); }, {
        .onProgress = {},
        .onComplete = [&fetched](std::optional<Error> error) { fetched.emplace(std::move(error)); },
    });
    (void)runtime.pumpUntil([&client] {
        client->downloader->update();
        return false;
    }, std::chrono::milliseconds(300));
    // clang-format on
    EXPECT_FALSE(fetched);
    EXPECT_LT(server.getSentBytes(), offer.downloadBytes / 8) << "Only the shard indexes download while the store falls behind, and no record.";

    browser.setUnwrittenBytes(0);
    // clang-format off
    ASSERT_TRUE(runtime.pumpUntil([&client, &fetched] {
        client->downloader->update();
        return fetched.has_value();
    }));
    // clang-format on
    EXPECT_FALSE(*fetched);
}

TEST_F(UpdaterTest, DownloadsOnlyWhatChanged) {
    publish(makeUpdate(2));
    const std::unique_ptr<Client> client = connect();
    const UpdateOffer offer = requireOffer(*client);

    EXPECT_LT(offer.downloadBytes, 200U * 1024U) << "The unchanged music and image come from the shards that ship with the app.";
    EXPECT_GT(offer.reusedBytes, 3U * kMegabyte) << "The offer reuses the bytes of the unchanged music and image.";
    server.clearExchanges();
    ASSERT_FALSE(install(*client, offer));
    EXPECT_LT(server.getSentBytes(), 300U * 1024U);

    EXPECT_GE(server.getSentBytes(), offer.downloadBytes);
}

TEST_F(UpdaterTest, RefusesAChannelThatGoesBack) {
    publish(makeUpdate(2));
    const std::filesystem::path channel = tree.getPath() / PublicationLayout::getChannelPath("stable");
    const std::filesystem::path older = store.getPath().parent_path() / (store.getPath().filename().string() + "-channel");
    std::filesystem::copy_file(channel, older);
    publish(makeUpdate(3));

    const std::unique_ptr<Client> client = connect();
    ASSERT_FALSE(install(*client, requireOffer(*client)));
    std::filesystem::copy_file(older, channel, std::filesystem::copy_options::overwrite_existing);
    std::filesystem::remove(older);

    auto [offer, error] = check(*client);
    EXPECT_FALSE(offer);
    ASSERT_TRUE(error);
    EXPECT_EQ(error->getCode(), Error::Code::ManifestRollbackRejected);
}

TEST_F(UpdaterTest, RefusesContentForLaterBuildsOfTheApp) {
    publish(makeUpdate(2), test::ReleaseFixture::kAppBuild + 1);
    const std::unique_ptr<Client> client = connect();

    auto [offer, error] = check(*client);
    EXPECT_FALSE(offer);
    ASSERT_TRUE(error);
    EXPECT_EQ(error->getCode(), Error::Code::ManifestIncompatible);
    EXPECT_EQ(error->getDetails().manifestId.size(), Digest::kSize * 2) << "The failure names the manifest it refused.";
    EXPECT_EQ(error->getDetails().platform, test::ReleaseFixture::kProfile);
}

TEST_F(UpdaterTest, KeepsTheContentWhenTheDeviceLacksRoom) {
    publish(makeUpdate(2));
    const auto client = std::make_unique<Client>(*this, std::make_unique<test::LimitedStoreFiles>(store.getPath(), 10 * kMegabyte));
    const std::optional<Error> error = install(*client, requireOffer(*client));

    ASSERT_TRUE(error);
    EXPECT_EQ(error->getCode(), Error::Code::InsufficientStorage);
    EXPECT_TRUE(error->isRetryable());
    EXPECT_GT(error->getDetails().requiredBytes, error->getDetails().availableBytes);
    EXPECT_EQ(error->getDetails().availableBytes, 10 * kMegabyte);
    EXPECT_EQ(error->getDetails().manifestId.size(), Digest::kSize * 2) << "The failure names the manifest of the install.";
    EXPECT_TRUE(client->package->getStore()->getState().active.isZero());
}

// A browser that refuses a record past its quota, after its estimate promised room, leaves the content as it was and reports the code that tells the app to free storage.
TEST_F(UpdaterTest, KeepsTheContentWhenTheBrowserRunsOutOfQuota) {
    Files update = makeUpdate(2);
    update["content/maps/world.bin"] = test::TestFiles::randomBytes(3 * kMegabyte, 22);
    publish(update);
    const auto client = std::make_unique<Client>(*this, std::make_unique<test::QuotaStoreFiles>(store.getPath(), kMegabyte));
    const std::optional<Error> error = install(*client, requireOffer(*client));

    ASSERT_TRUE(error);
    EXPECT_EQ(error->getCode(), Error::Code::QuotaExceeded);
    EXPECT_EQ(Error::getCodeName(error->getCode()), "quotaExceeded");
    EXPECT_TRUE(error->isRetryable());
    EXPECT_TRUE(client->package->getStore()->getState().active.isZero());
}

TEST_F(UpdaterTest, ResumesAnUpdateThatStoppedAndCommitsOnlyOnceItIsWhole) {
    // A large new file spans several records, and the connection drops in the middle of one of them.
    Files update = makeUpdate(2);
    update["content/maps/world.bin"] = test::TestFiles::randomBytes(6 * kMegabyte, 21);
    publish(update);
    std::uint64_t whole = 0;
    {
        const std::unique_ptr<Client> probe = connect();
        whole = requireOffer(*probe).downloadBytes;
    }

    server.dropAfter(1500 * 1024, 1);
    {
        const std::unique_ptr<Client> client = connect();
        const std::optional<Error> error = install(*client, requireOffer(*client));
        ASSERT_TRUE(error);
        EXPECT_TRUE(error->isRetryable()) << static_cast<int>(error->getCode()) << " " << error->what();
        EXPECT_TRUE(client->package->getStore()->getState().active.isZero());
        const std::filesystem::path staging = store.getPath() / "staging";
        EXPECT_TRUE(runtime.pumpUntil([&staging] { return std::filesystem::exists(staging) && !std::filesystem::is_empty(staging); })) << "The record that stopped keeps its first bytes.";
    }

    // The app starts again with the content it had and collects what no content uses, which keeps the records of the install that stopped, and the next attempt downloads only what is missing, from the middle of the record that stopped.
    server.clearExchanges();
    const std::unique_ptr<Client> client = connect();
    EXPECT_EQ(collect(*client).removedRecords, 0U);
    EXPECT_EQ(readText(*client->package, "content/data/config.json"), R"({"level": 1})");
    const UpdateOffer offer = requireOffer(*client);
    EXPECT_LT(offer.downloadBytes, whole);
    ASSERT_FALSE(install(*client, offer));
    EXPECT_GE(client->downloader->getStatistics().resumedRecords, 1U);
    EXPECT_LT(server.getSentBytes(), whole);

    const std::unique_ptr<Client> restarted = connect();
    EXPECT_EQ(restarted->package->read("content/maps/world.bin"), update.at("content/maps/world.bin"));
}

TEST_F(UpdaterTest, FallsBackToThePreviousContentWhenTheActiveOneIsDamaged) {
    publish(makeUpdate(2));
    Digest second;
    {
        const std::unique_ptr<Client> client = connect();
        ASSERT_FALSE(install(*client, requireOffer(*client)));
        publish(makeUpdate(3));
        const UpdateOffer offer = requireOffer(*client);
        second = offer.content->manifest->getId();
        ASSERT_FALSE(install(*client, offer));
    }
    std::filesystem::resize_file(store.getPath() / "manifests" / (second.toHex() + ".hmanifest"), 100);

    const std::unique_ptr<Client> client = connect();
    EXPECT_EQ(readText(*client->package, "content/data/config.json"), R"({"level": 2})");
    const std::optional<ReleasePackage::Recovery> recovery = client->package->takeRecovery();
    ASSERT_TRUE(recovery);
    EXPECT_EQ(recovery->failed, second);
    EXPECT_FALSE(client->package->takeRecovery()) << "A fallback is reported once.";
    EXPECT_FALSE(client->package->getStore()->getState().active.isZero());
    EXPECT_TRUE(client->package->getStore()->getState().previous.isZero());
}

TEST_F(UpdaterTest, FallsBackToTheContentThatShipsWhenNoDownloadIsUsable) {
    publish(makeUpdate(2));
    {
        const std::unique_ptr<Client> client = connect();
        ASSERT_FALSE(install(*client, requireOffer(*client)));
    }
    std::filesystem::remove_all(store.getPath() / "manifests");

    const std::unique_ptr<Client> client = connect();
    EXPECT_EQ(readText(*client->package, "content/data/config.json"), R"({"level": 1})");
    ASSERT_TRUE(client->package->takeRecovery());
    EXPECT_TRUE(client->package->getStore()->getState().active.isZero());
}

TEST_F(UpdaterTest, ReplacesARecordThatTheDeviceDamaged) {
    publish(makeUpdate(2));
    {
        const std::unique_ptr<Client> client = connect();
        ASSERT_FALSE(install(*client, requireOffer(*client)));
    }
    for (const std::filesystem::directory_entry& segment : std::filesystem::directory_iterator(store.getPath() / "segments")) {
        std::filesystem::resize_file(segment.path(), std::filesystem::file_size(segment.path()) - 1);
    }

    // The record that lost its end leaves the store when the store opens, so the content needs it again.
    const std::unique_ptr<Client> client = connect();
    const std::shared_ptr<const ReleasePackage::Content> content = client->package->getContent();
    EXPECT_THROW((void)client->package->read("content/maps/level.tmj"), Error);
    std::optional<std::optional<Error>> fetched;
    const std::vector<Catalog::Chunk> missing = Downloader::findMissing(*content->shards, Updater::getChunks(*content->catalog, Delivery::Required, {}));
    EXPECT_FALSE(missing.empty());
    client->downloader->fetch(content->shards, missing, [client = client.get()](const ShardReference& pack) { return client->updater->locate(pack); }, {.onComplete = [&fetched](std::optional<Error> error) { fetched.emplace(std::move(error)); }});
    ASSERT_TRUE(runtime.pumpUntil([&fetched] { return fetched.has_value(); }));
    ASSERT_FALSE(*fetched);
    EXPECT_EQ(client->package->read("content/maps/level.tmj"), makeUpdate(2).at("content/maps/level.tmj"));
}

TEST_F(UpdaterTest, ReadsInWhatTheStoreHoldsInsteadOfDownloadingItAgain) {
    publish(makeUpdate(2));
    {
        const auto client = std::make_unique<Client>(*this, std::make_unique<test::BrowserStoreFiles>(store.getPath()));
        ASSERT_FALSE(install(*client, requireOffer(*client)));
    }

    // A store of the web reads records in only when they are needed, so a new visit starts with none of them readable.
    auto files = std::make_unique<test::BrowserStoreFiles>(store.getPath());
    test::BrowserStoreFiles& browser = *files;
    const auto client = std::make_unique<Client>(*this, std::move(files));
    try {
        (void)client->package->read("content/maps/level.tmj");
        ADD_FAILURE() << "A record that the store did not read in is not readable.";
    } catch (const Error& error) {
        // The failure names the file that the app read, the pack and the record it needed.
        EXPECT_EQ(error.getDetails().path, "content/maps/level.tmj");
        EXPECT_EQ(error.getDetails().packId.size(), Digest::kSize * 2);
        EXPECT_EQ(error.getDetails().storedId.size(), Digest::kSize * 2);
        EXPECT_EQ(error.getDetails().contentId.size(), Digest::kSize * 2);
    }
    const std::shared_ptr<const ReleasePackage::Content> content = client->package->getContent();
    const std::vector<Catalog::Chunk> missing = Downloader::findMissing(*content->shards, Updater::getChunks(*content->catalog, Delivery::Required, {}));
    ASSERT_FALSE(missing.empty());
    EXPECT_EQ(Downloader::measure(missing, *client->package->getStore()), 0U) << "Reading records in downloads nothing.";
    server.clearExchanges();

    std::optional<std::optional<Error>> fetched;
    std::uint64_t total = 0;
    // clang-format off
    client->downloader->fetch(content->shards, missing, [&client](const ShardReference& pack) { return client->updater->locate(pack); }, {
        .onProgress = [&total](const Downloader::Progress& progress) { total = std::max(total, progress.totalBytes); },
        .onComplete = [&fetched](std::optional<Error> error) { fetched.emplace(std::move(error)); },
    }, Downloader::Priority::Foreground, Downloader::Use::Read);
    // clang-format on
    ASSERT_TRUE(runtime.pumpUntil([&fetched] { return fetched.has_value(); }));
    ASSERT_FALSE(*fetched);
    EXPECT_EQ(total, 0U);
    EXPECT_EQ(server.getExchanges().size(), 0U) << "Nothing downloads that the store holds.";
    EXPECT_EQ(browser.getLoadedCount(), missing.size());
    EXPECT_EQ(client->package->read("content/maps/level.tmj"), makeUpdate(2).at("content/maps/level.tmj"));
}

TEST_F(UpdaterTest, LetsBackgroundWorkHoldOneRequestAndTheForegroundGoFirst) {
    Files update = game;
    for (const char* name : {"content/extra/a.bin", "content/extra/b.bin", "content/extra/c.bin"}) {
        update[name] = test::TestFiles::randomBytes(200 * 1024, static_cast<std::uint64_t>(name[14]));
    }
    publish(update);
    const std::unique_ptr<Client> client = connect();
    const UpdateOffer offer = requireOffer(*client);
    const auto locate = [&client](const ShardReference& pack) { return client->updater->locate(pack); };
    std::vector<std::string> finished;
    // clang-format off
    const auto chunksOf = [&offer](std::string_view path) {
        const Catalog& catalog = *offer.content->catalog;
        const Catalog::File file = catalog.getFile(*catalog.findFile(path));
        std::vector<Catalog::Chunk> chunks;
        for (std::uint64_t part = file.firstPart; part < file.firstPart + file.partCount; ++part) {
            chunks.push_back(catalog.getChunk(catalog.getPart(part).chunk));
        }
        return chunks;
    };
    const auto fetch = [&](std::string_view path, Downloader::Priority priority) {
        client->downloader->fetch(offer.content->shards, chunksOf(path), locate, {.onComplete = [&finished, name = std::string(path)](std::optional<Error> error) {
            EXPECT_FALSE(error) << error->what();
            finished.push_back(name);
        }}, priority);
    };
    // clang-format on
    server.clearExchanges();
    server.setDelay(150);

    // The first background request reads the header and the index of the shard before its range starts, so the requests after it plan ranges of their own.
    fetch("content/extra/a.bin", Downloader::Priority::Background);
    ASSERT_TRUE(runtime.pumpUntil([&client] { return client->downloader->getStatistics().requests >= 3; }));
    fetch("content/extra/b.bin", Downloader::Priority::Background);
    fetch("content/extra/c.bin", Downloader::Priority::Foreground);
    ASSERT_TRUE(runtime.pumpUntil([&finished] { return finished.size() == 3; }));

    EXPECT_EQ(finished.back(), "content/extra/b.bin") << "The second background request starts only once the first ends.";
    EXPECT_EQ(server.getMostConcurrent(), 2) << "The second background request waits for the first, while the foreground one runs next to it.";
}

TEST_F(UpdaterTest, SharesOneDownloadAmongRequestsAndKeepsItForTheOnesThatStay) {
    publish(makeUpdate(2));
    const std::unique_ptr<Client> client = connect();
    const UpdateOffer offer = requireOffer(*client);
    server.clearExchanges();

    std::optional<std::optional<Error>> first;
    std::optional<std::optional<Error>> second;
    const auto locate = [&client](const ShardReference& pack) { return client->updater->locate(pack); };
    const std::uint64_t cancelled = client->downloader->fetch(offer.content->shards, offer.missing, locate, {.onComplete = [&first](std::optional<Error> error) { first.emplace(std::move(error)); }});
    client->downloader->fetch(offer.content->shards, offer.missing, locate, {.onComplete = [&second](std::optional<Error> error) { second.emplace(std::move(error)); }});
    client->downloader->cancel(cancelled);
    ASSERT_TRUE(runtime.pumpUntil([&first, &second] { return first.has_value() && second.has_value(); }));

    ASSERT_TRUE(*first);
    EXPECT_EQ((*first)->getCode(), Error::Code::NetworkCancelled);
    EXPECT_FALSE(*second);
    EXPECT_LE(server.getSentBytes(), offer.downloadBytes + 64 * 1024) << "Both requests share every download.";
}

// A record whose bytes fail their checks fails its request and stops being wanted, so a request after the server serves it right downloads it again.
TEST_F(UpdaterTest, DownloadsARecordAgainAfterItFailedItsChecks) {
    const ChannelPublisher::Publication publication = publish(makeUpdate(2));

    // Every new pack serves records with a damaged byte in every block, while its header and its index stay whole.
    std::map<std::string, std::string> originals;
    for (const ShardReference& reference : publication.newPacks) {
        const std::string pack = PublicationLayout::getPackPath(reference);
        std::ifstream stream(tree.getPath() / pack, std::ios::binary);
        std::string bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
        originals.emplace(pack, bytes);
        std::array<std::uint8_t, ShardHeader::kSize> header{};
        std::copy_n(bytes.begin(), header.size(), reinterpret_cast<char*>(header.data()));
        const std::uint64_t indexOffset = ShardHeader::parse(header, bytes.size()).indexOffset;
        for (std::uint64_t offset = ShardHeader::kSize + 64; offset < indexOffset; offset += 1024) {
            bytes[offset] = static_cast<char>(~bytes[offset]);
        }
        tree.write(pack, bytes);
        server.touch(pack);
    }

    const std::unique_ptr<Client> client = connect();
    const UpdateOffer offer = requireOffer(*client);
    const auto locate = [&client](const ShardReference& reference) { return client->updater->locate(reference); };
    std::optional<std::optional<Error>> first;
    client->downloader->fetch(offer.content->shards, offer.missing, locate, {.onComplete = [&first](std::optional<Error> error) { first.emplace(std::move(error)); }});
    ASSERT_TRUE(runtime.pumpUntil([&first] { return first.has_value(); }));
    ASSERT_TRUE(*first);

    // Records that the damaged pack sent before may still fail their checks, but every attempt settles, and one that the whole pack serves succeeds.
    for (const auto& [pack, bytes] : originals) {
        tree.write(pack, bytes);
        server.touch(pack);
    }
    std::optional<Error> failure;
    for (int attempt = 0; attempt < 3; ++attempt) {
        std::optional<std::optional<Error>> retried;
        client->downloader->fetch(offer.content->shards, offer.missing, locate, {.onComplete = [&retried](std::optional<Error> error) { retried.emplace(std::move(error)); }});
        ASSERT_TRUE(runtime.pumpUntil([&retried] { return retried.has_value(); }));
        failure = *retried;
        if (!failure) {
            break;
        }
    }
    EXPECT_FALSE(failure);
}

TEST_F(UpdaterTest, CollectsWhatNoRetainedContentUses) {
    publish(makeUpdate(2));
    const std::unique_ptr<Client> client = connect();
    for (const int level : {2, 3, 4}) {
        if (level > 2) {
            publish(makeUpdate(level));
        }
        ASSERT_FALSE(install(*client, requireOffer(*client)));
    }
    const LocalStore::Statistics before = client->package->getStore()->getStatistics();

    EXPECT_GT(collect(*client).removedRecords, 0U);
    EXPECT_LT(client->package->getStore()->getStatistics().records, before.records);
    const std::unique_ptr<Client> restarted = connect();
    EXPECT_EQ(readText(*restarted->package, "content/data/config.json"), R"({"level": 4})");
    EXPECT_EQ(restarted->package->read("content/maps/level.tmj"), makeUpdate(4).at("content/maps/level.tmj"));
}

} // namespace haylen::content
