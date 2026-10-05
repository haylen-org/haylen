#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "content/ReleaseBuilder.hpp"
#include "content/ReleaseInspector.hpp"
#include "content/ReleasePackage.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "support/ReleaseFixture.hpp"
#include "support/TemporaryDirectory.hpp"
#include "support/TestFiles.hpp"

namespace haylen::content {

// The release scenarios that a shipped app goes through: a small offline game, a game of many shards and an update of code and content between two builds of a store.
class ReleaseAcceptanceTest : public ::testing::Test {
  protected:
    using Files = std::map<std::string, std::vector<std::uint8_t>>;

    static constexpr std::size_t kMegabyte = 1024 * 1024;
    static constexpr std::string_view kMenuSource = "local menu = {title = 'The menu of the release test'}\nreturn menu";

    [[nodiscard]] static Files makeGame() {
        Files files;
        const auto text = [&files](const std::string& path, std::string_view content) { files.emplace(path, test::TestFiles::bytes(std::string(content))); };
        text("app.json", R"({"name": "Acceptance", "identifier": "dev.haylen.tests", "autoload": ["state.player"]})");
        text("source/main.lua", "local menu = require('scenes.menu')\nloaded = menu.title");
        text("source/scenes/menu.lua", kMenuSource);
        text("source/scenes/island.lua", "return {title = 'The island of the release test'}");
        text("source/state/player.lua", "return {name = 'The player of the release test'}");
        text("content/data/enemies.json", R"({"enemies": ["the slime of the release test", "the bat of the release test"]})");
        files.emplace("content/images/hero.png", test::TestFiles::pngImage(64, 64, 0x3366CCFF));
        files.emplace("content/music/theme.ogg", test::TestFiles::randomBytes(kMegabyte, 99));
        return files;
    }

    [[nodiscard]] std::map<std::string, std::vector<std::uint8_t>> readRelease() const {
        std::map<std::string, std::vector<std::uint8_t>> files;
        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(release.getFolder())) {
            std::ifstream stream(entry.path(), std::ios::binary);
            files.emplace(entry.path().filename().string(), std::vector<std::uint8_t>{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()});
        }
        return files;
    }

    [[nodiscard]] static bool holds(const std::vector<std::uint8_t>& file, std::span<const std::uint8_t> part) {
        return std::search(file.begin(), file.end(), part.begin(), part.end()) != file.end();
    }

    [[nodiscard]] ReleaseInspector makeInspector() const {
        return {release.getKeys(), {release.getSigningKey().getVerifyingKey()}};
    }

    test::ReleaseFixture release;
};

TEST_F(ReleaseAcceptanceTest, ShipsASmallGameWithoutAnyReadableFile) {
    const Files game = makeGame();
    release.build(io::MemoryPackage("source", game));

    // The release is the two manifests and one shard of each domain, and no file of it holds a path, a line of code or a byte of an asset.
    const auto shipped = readRelease();
    ASSERT_EQ(shipped.size(), 4U);
    EXPECT_TRUE(shipped.contains(std::string(ReleasePackage::kAppManifestFile)));
    EXPECT_TRUE(shipped.contains(std::string(ReleasePackage::kContentManifestFile)));
    for (const auto& [name, bytes] : shipped) {
        for (const auto& [path, content] : game) {
            EXPECT_FALSE(holds(bytes, test::TestFiles::bytes(path))) << name << " names " << path;
            EXPECT_FALSE(holds(bytes, std::span(content).first(std::min<std::size_t>(content.size(), 24)))) << name << " holds the start of " << path;
        }
        EXPECT_FALSE(holds(bytes, test::TestFiles::bytes("release test"))) << name;
    }
    EXPECT_EQ(makeInspector().verify(release.getFolder()).files, game.size());
    EXPECT_EQ(release.open()->readAsset("images/hero.png"), game.at("content/images/hero.png"));
}

TEST_F(ReleaseAcceptanceTest, SplitsALargeGameIntoShardsAndPatchesOnlyWhatChanged) {
    // A small shard target stands for the target of a game of many gigabytes, so the content spans many shards.
    Files game = makeGame();
    for (int index = 0; index < 12; ++index) {
        game.emplace("content/world/region" + std::to_string(index) + ".bin", test::TestFiles::randomBytes(2 * kMegabyte, static_cast<std::uint64_t>(index + 1)));
    }
    const std::uint64_t target = 4 * kMegabyte;
    release.build(io::MemoryPackage("source", game), target);
    const ReleaseInspector::Report base = makeInspector().inspect(release.getFolder());
    ASSERT_GE(base.content.envelope.shards.size(), 6U);
    const auto before = readRelease();

    game["content/world/region5.bin"][kMegabyte] ^= 1;
    release.build(io::MemoryPackage("source", game), target);
    const ReleaseInspector::Report update = makeInspector().inspect(release.getFolder());
    const ReleaseInspector::Difference difference = ReleaseInspector::compare(base.content, update.content);
    EXPECT_EQ(difference.newShards.size(), 1U);
    EXPECT_LE(difference.downloadBytes, 4 * kMegabyte) << "An update downloads the chunks around the change, never the game.";
    EXPECT_EQ(difference.changedFiles, std::vector<std::string>{"content/world/region5.bin"});

    // Every shard of the base stays byte for byte in the update.
    const auto after = readRelease();
    for (const ShardReference& shard : base.content.envelope.shards) {
        ASSERT_TRUE(after.contains(shard.getFileName()));
        EXPECT_EQ(after.at(shard.getFileName()), before.at(shard.getFileName()));
    }
    EXPECT_EQ(release.open()->readAsset("world/region5.bin"), game.at("content/world/region5.bin"));
}

TEST_F(ReleaseAcceptanceTest, UpdatesCodeAndContentWithNewShardsOnly) {
    Files game = makeGame();
    release.build(io::MemoryPackage("source", game));
    const auto before = readRelease();

    // The next build of the store changes one Lua module and one asset, so it adds one shard to each domain and two new manifests, and every earlier file stays as it was.
    game["source/scenes/island.lua"] = test::TestFiles::bytes("return {title = 'The changed island of the release test'}");
    game["content/data/enemies.json"] = test::TestFiles::bytes(R"({"enemies": ["the ghost of the release test"]})");
    release.build(io::MemoryPackage("source", game));
    const auto after = readRelease();

    std::set<std::string> added;
    for (const auto& [name, bytes] : after) {
        const auto found = before.find(name);
        if (found == before.end()) {
            added.insert(name);
        } else if (!name.ends_with(".hmanifest")) {
            EXPECT_EQ(bytes, found->second) << name;
        }
    }
    EXPECT_EQ(added.size(), 2U) << "Each domain adds one shard.";
    EXPECT_EQ(release.getStatistics(Manifest::Domain::App).newChunks, 1U);
    EXPECT_EQ(release.getStatistics(Manifest::Domain::Content).newChunks, 1U);
    EXPECT_TRUE(release.open()->isLuaBytecode("source/scenes/island.lua"));
}

} // namespace haylen::content
