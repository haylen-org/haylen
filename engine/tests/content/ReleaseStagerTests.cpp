#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "content/ReleaseInspector.hpp"
#include "content/ReleaseStager.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "support/ReleaseFixture.hpp"
#include "support/TemporaryDirectory.hpp"
#include "support/TestFiles.hpp"

namespace haylen::content {

// How a release splits for the stores that deliver it: what ships with the app, what stays for the server and the packs of named content, such as the expansions that a store installs on their own, and how little of it changes between two builds.
class ReleaseStagerTest : public ::testing::Test {
  protected:
    using Files = std::map<std::string, std::vector<std::uint8_t>>;

    static constexpr std::string_view kApp = R"({"name": "Packs", "identifier": "dev.haylen.tests", "content": {"delivery": [{"match": "forest/**", "pack": "forest"}, {"match": "music/**", "mode": "prefetch"}]}})";

    ReleaseStagerTest() {
        files.emplace("app.json", test::TestFiles::bytes(std::string(kApp)));
        files.emplace("source/main.lua", test::TestFiles::bytes("return {level = 1}"));
        files.emplace("source/scenes/menu.lua", test::TestFiles::bytes("return {title = 'menu'}"));
        files.emplace("content/data/config.json", test::TestFiles::bytes(R"({"level": 1})"));
        files.emplace("content/images/hero.png", test::TestFiles::randomBytes(300 * 1024, 1));
        files.emplace("content/forest/map.tmj", test::TestFiles::randomBytes(200 * 1024, 2));
        files.emplace("content/music/theme.ogg", test::TestFiles::randomBytes(400 * 1024, 3));
    }

    [[nodiscard]] ReleaseStager::Layout stage(const std::filesystem::path& folder, ReleaseStager::Content content = ReleaseStager::Content::Required) const {
        const ReleaseInspector inspector(release.getKeys(), {release.getSigningKey().getVerifyingKey()});
        const core::AppConfig config = core::AppConfig::fromJson(core::Json::parse(kApp));
        return ReleaseStager(inspector, config.content).stage(release.getFolder(), folder / "shipped", folder / "deferred", folder / "packs", content);
    }

    [[nodiscard]] static std::map<std::string, std::vector<std::uint8_t>> readFolder(const std::filesystem::path& folder) {
        std::map<std::string, std::vector<std::uint8_t>> read;
        for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(folder)) {
            if (entry.is_regular_file()) {
                std::ifstream stream(entry.path(), std::ios::binary);
                read.emplace(entry.path().lexically_relative(folder).generic_string(), std::vector<std::uint8_t>{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()});
            }
        }
        return read;
    }

    test::ReleaseFixture release;
    Files files;
    test::TemporaryDirectory work;
};

TEST_F(ReleaseStagerTest, ShipsTheRequiredContentAndGivesEachPackItsOwnShards) {
    release.build(io::MemoryPackage("source", files));
    const ReleaseStager::Layout layout = stage(work.getPath());

    ASSERT_EQ(layout.packs.size(), 1U);
    ASSERT_EQ(layout.packs.at("forest").size(), 1U);
    EXPECT_EQ(layout.deferred.size(), 1U) << "The prefetched music stays for the server.";
    EXPECT_NE(std::ranges::find(layout.shipped, "app.hmanifest"), layout.shipped.end());
    EXPECT_NE(std::ranges::find(layout.shipped, "content.hmanifest"), layout.shipped.end());
    EXPECT_TRUE(std::filesystem::exists(work.getPath() / "packs" / "forest" / layout.packs.at("forest").front()));
    EXPECT_TRUE(std::filesystem::exists(work.getPath() / "deferred" / layout.deferred.front()));

    // An app without a server ships the later deliveries too, while a pack always goes to its store.
    test::TemporaryDirectory everything;
    const ReleaseStager::Layout all = stage(everything.getPath(), ReleaseStager::Content::All);
    EXPECT_TRUE(all.deferred.empty());
    EXPECT_EQ(all.shipped.size(), layout.shipped.size() + 1);
    EXPECT_EQ(all.packs.at("forest"), layout.packs.at("forest"));

    test::TemporaryDirectory web;
    const ReleaseStager::Layout page = stage(web.getPath(), ReleaseStager::Content::None);
    EXPECT_EQ(page.shipped.size(), layout.shipped.size() - 1) << "A web page downloads the manifests and the app domain alone.";
}

TEST_F(ReleaseStagerTest, ChangesOnlyTheShardsOfWhatAnUpdateChanged) {
    release.build(io::MemoryPackage("source", files));
    const std::filesystem::path before = work.getPath() / "before";
    (void)stage(before);

    // A new build of a store changes one Lua module and one small asset, and its depots keep every other file byte for byte.
    files["source/scenes/menu.lua"] = test::TestFiles::bytes("return {title = 'new menu'}");
    files["content/data/config.json"] = test::TestFiles::bytes(R"({"level": 2})");
    release.build(io::MemoryPackage("source", files));
    const std::filesystem::path after = work.getPath() / "after";
    (void)stage(after);

    const auto earlier = readFolder(before);
    const auto later = readFolder(after);
    std::vector<std::string> added;
    std::size_t kept = 0;
    for (const auto& [path, bytes] : later) {
        const auto found = earlier.find(path);
        if (path.ends_with(".hmanifest")) {
            EXPECT_NE(found->second, bytes) << "The manifest " << path << " names the new shards.";
        } else if (found == earlier.end()) {
            added.push_back(path);
        } else {
            EXPECT_EQ(found->second, bytes) << "The shard " << path << " keeps its name only with its bytes.";
            ++kept;
        }
    }
    EXPECT_EQ(readFolder(before / "packs"), readFolder(after / "packs")) << "The pack depot does not change.";
    EXPECT_EQ(readFolder(before / "deferred"), readFolder(after / "deferred"));
    EXPECT_GE(kept, 2U) << "The unchanged shards of the forest and the music stay as they were.";
    EXPECT_LE(added.size(), 2U) << "Only the new shards of each domain are new.";
}

} // namespace haylen::content
