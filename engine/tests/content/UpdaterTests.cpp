#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
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
#include "haylen/content/Error.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "support/BrowserStoreFiles.hpp"
#include "support/ContentServer.hpp"
#include "support/LimitedStoreFiles.hpp"
#include "support/ReleaseFixture.hpp"
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
        Client(UpdaterTest& test, std::unique_ptr<StoreFiles> storeFiles) : jobs(test.jobSystems.emplace_back(test.runtime.getRuntime(), [](const std::string& error) { ADD_FAILURE() << error; })), transport(test.runtime.getRuntime()) {
            const VerifyingKey trusted = test.release.getSigningKey().getVerifyingKey();
            auto recordStore = std::make_shared<LocalStore>(std::move(storeFiles), test.release.getCompatibility().application, test::ReleaseFixture::kAppBuild);
            package = ReleasePackage::open(test.release.getFiles(), test.release.getKeys(), std::span(&trusted, 1), test.release.getCompatibility(), std::move(recordStore));
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
        const ChannelPublisher publisher(release.getKeys(), release.getContentKeyId(), release.getSharedSigningKey());
        return publisher.publish(io::MemoryPackage("source", files), tree.getPath(), {.profile = std::string(test::ReleaseFixture::kProfile), .appBuild = appBuild, .channel = "stable", .release = release.getFolder()});
    }

    [[nodiscard]] Files makeUpdate(int level) const {
        Files files = game;
        files["content/data/config.json"] = test::TestFiles::bytes(R"({"level": )" + std::to_string(level) + "}");
        files["content/maps/level.tmj"] = test::TestFiles::randomBytes(100 * 1024, static_cast<std::uint64_t>(level));
        return files;
    }

    [[nodiscard]] std::unique_ptr<Client> connect() {
        return std::make_unique<Client>(*this, std::make_unique<FolderStoreFiles>(store.getPath()));
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

TEST_F(UpdaterTest, DownloadsOnlyWhatChanged) {
    publish(makeUpdate(2));
    const std::unique_ptr<Client> client = connect();
    const UpdateOffer offer = requireOffer(*client);

    EXPECT_LT(offer.downloadBytes, 200U * 1024U) << "The unchanged music and image come from the shards that ship with the app.";
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
    EXPECT_THROW((void)client->package->read("content/maps/level.tmj"), Error);
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
    });
    // clang-format on
    ASSERT_TRUE(runtime.pumpUntil([&fetched] { return fetched.has_value(); }));
    ASSERT_FALSE(*fetched);
    EXPECT_EQ(total, 0U);
    EXPECT_EQ(server.getExchanges().size(), 0U) << "Nothing downloads that the store holds.";
    EXPECT_EQ(browser.getLoadedCount(), missing.size());
    EXPECT_EQ(client->package->read("content/maps/level.tmj"), makeUpdate(2).at("content/maps/level.tmj"));
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
