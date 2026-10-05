#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "content/RecordCache.hpp"
#include "content/ReleaseBuilder.hpp"
#include "content/ReleasePackage.hpp"
#include "content/format/Chunker.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "io/MemoryReader.hpp"
#include "support/ReleaseFixture.hpp"
#include "support/TemporaryDirectory.hpp"
#include "support/TestFiles.hpp"

namespace haylen::content {

class ReleaseBuilderTest : public ::testing::Test {
  protected:
    using Files = std::map<std::string, std::vector<std::uint8_t>>;

    static constexpr std::size_t kMegabyte = 1024 * 1024;
    static constexpr std::string_view kAppJson = R"({"name": "Release Test", "identifier": "dev.haylen.tests", "version": "1.0.0", "plugins": {"ads": {}}})";

    // A package whose files read as other bytes the second time they open, like files that change while a release builds.
    class ChangingPackage final : public io::Package {
      public:
        explicit ChangingPackage(Files contents) : files(std::move(contents)) {}

        [[nodiscard]] std::string_view getName() const noexcept override {
            return "changing";
        }
        [[nodiscard]] bool exists(std::string_view path) const override {
            return files.contains(std::string(path));
        }
        [[nodiscard]] std::uint64_t getFileSize(std::string_view path) const override {
            return files.at(std::string(path)).size();
        }
        [[nodiscard]] std::unique_ptr<io::PackageReader> openReader(std::string_view path) const override {
            std::vector<std::uint8_t> bytes = files.at(std::string(path));
            if (++opened[std::string(path)] > 1 && !bytes.empty()) {
                bytes.back() ^= 1;
            }
            return std::make_unique<io::MemoryReader>(std::make_shared<const std::vector<std::uint8_t>>(std::move(bytes)));
        }
        [[nodiscard]] std::vector<std::string> list(std::string_view folder) const override {
            std::vector<std::string> paths;
            for (const auto& [path, bytes] : files) {
                if (path.starts_with(std::string(folder) + "/")) {
                    paths.push_back(path);
                }
            }
            return paths;
        }

      private:
        Files files;
        mutable std::map<std::string, int> opened;
    };

    [[nodiscard]] static Files makeFiles() {
        Files files;
        const auto text = [&files](const std::string& path, const std::string& content) { files.emplace(path, test::TestFiles::bytes(content)); };
        text("app.json", std::string(kAppJson));
        text("source/main.lua", "loaded = true");
        text("plugins/ads/plugin.json", R"({"id": "ads", "version": "1.0.0"})");
        text("plugins/ads/source/init.lua", "return {}");
        text("content/data/config.json", R"({"level": 3})");
        files.emplace("content/world/terrain.bin", test::TestFiles::randomBytes(9 * kMegabyte, 1));
        return files;
    }

    ReleaseBuilder::Result build(const io::Package& source, const std::string& name, const std::string& earlier = "", std::shared_ptr<const RecordCache> cache = nullptr, std::uint64_t shardTarget = ContentBuilder::kDefaultShardTarget) const {
        const ReleaseBuilder builder(fixture.getKeys(), fixture.getContentKey()->getId(), std::make_shared<const SigningKey>(test::ReleaseFixture::makeKey(101)), std::move(cache));
        const std::optional<std::filesystem::path> previous = earlier.empty() ? std::nullopt : std::optional(directory.getPath() / earlier);
        return builder.build(source, directory.getPath() / name, previous, {.profile = std::string(test::ReleaseFixture::kProfile), .appBuild = test::ReleaseFixture::kAppBuild, .shardTarget = shardTarget});
    }

    [[nodiscard]] std::map<std::string, std::vector<std::uint8_t>> readFolder(const std::string& name) const {
        std::map<std::string, std::vector<std::uint8_t>> files;
        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory.getPath() / name)) {
            std::ifstream stream(entry.path(), std::ios::binary);
            files.emplace(entry.path().filename().string(), std::vector<std::uint8_t>{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()});
        }
        return files;
    }

    [[nodiscard]] std::unique_ptr<io::Package> open(const std::string& name) const {
        return fixture.open(io::Package::openDirectory(directory.getPath() / name));
    }

    test::ReleaseFixture fixture;
    test::TemporaryDirectory directory;
};

TEST_F(ReleaseBuilderTest, ReleasesOnlyThePackageOfTheApp) {
    // The folder of an app also holds its platform projects, notes, plugins it does not list and files of file managers, none of which ship.
    const test::TemporaryDirectory app;
    for (const auto& [path, bytes] : makeFiles()) {
        app.write(path, std::string(bytes.begin(), bytes.end()));
    }
    app.write("plugins/unused/plugin.json", R"({"id": "unused", "version": "1.0.0"})");
    app.write("plugins/unused/source/init.lua", "return {}");
    app.write("plugins/ads/android/build.gradle.kts", "plugins {}");
    app.write("platform/apple/project.yml", "name: App");
    app.write("content/.DS_Store", "finder");
    app.write("notes.txt", "notes");
    (void)build(*io::Package::openDirectory(app.getPath()), "release");

    const std::unique_ptr<io::Package> release = open("release");
    EXPECT_EQ(release->list(""), (std::vector<std::string>{"app.json", "content/data/config.json", "content/world/terrain.bin", "plugins/ads/plugin.json", "plugins/ads/source/init.lua", "source/main.lua"}));
    for (const auto& [name, bytes] : readFolder("release")) {
        EXPECT_TRUE(name.ends_with(".hpak") || name.ends_with(".hmanifest")) << name;
    }
}

TEST_F(ReleaseBuilderTest, BuildsTheSameBytesWithAndWithoutItsCache) {
    const io::MemoryPackage source("source", makeFiles());
    const auto cache = std::make_shared<const RecordCache>(directory.getPath() / "cache");
    (void)build(source, "plain");
    const ReleaseBuilder::Result cold = build(source, "cold", "", cache);
    const ReleaseBuilder::Result warm = build(source, "warm", "", cache);

    EXPECT_EQ(cold.content.statistics.cachedChunks, 0U);
    EXPECT_EQ(warm.content.statistics.cachedChunks, warm.content.statistics.newChunks) << "A warm cache seals no chunk again.";
    EXPECT_EQ(warm.app.statistics.cachedChunks, warm.app.statistics.newChunks);
    EXPECT_EQ(readFolder("cold"), readFolder("plain"));
    EXPECT_EQ(readFolder("warm"), readFolder("plain"));
}

TEST_F(ReleaseBuilderTest, KeepsUnchangedShardsByteForByte) {
    Files files = makeFiles();
    const ReleaseBuilder::Result base = build(io::MemoryPackage("source", files), "base");
    const auto before = readFolder("base");

    files["content/data/config.json"] = test::TestFiles::bytes(R"({"level": 4})");
    const ReleaseBuilder::Result update = build(io::MemoryPackage("source", files), "update", "base");
    const auto after = readFolder("update");

    EXPECT_EQ(update.app.statistics.newChunks, 0U);
    EXPECT_EQ(update.content.statistics.newChunks, 1U);
    EXPECT_EQ(after.at(std::string(ReleasePackage::kAppManifestFile)), before.at(std::string(ReleasePackage::kAppManifestFile))) << "An unchanged domain signs the same manifest.";
    for (const ShardReference& shard : base.content.shards) {
        EXPECT_EQ(after.at(shard.getFileName()), before.at(shard.getFileName())) << "Unrelated shards stay byte for byte.";
    }
    ASSERT_EQ(update.content.shards.size(), base.content.shards.size() + 1);
    EXPECT_EQ(update.content.keptShards, base.content.shards.size());
    EXPECT_LT(update.content.shards.back().fileSize, 4096U) << "The new shard holds only the changed chunk.";
    EXPECT_EQ(after.size(), before.size() + 1);
    EXPECT_EQ(open("update")->readAssetText("data/config.json"), R"({"level": 4})");
}

TEST_F(ReleaseBuilderTest, RepacksShardsThatLostMostOfTheirBytes) {
    Files files = makeFiles();
    files.emplace("content/world/rivers.bin", test::TestFiles::randomBytes(9 * kMegabyte, 2));
    const ReleaseBuilder::Result base = build(io::MemoryPackage("source", files), "base");
    ASSERT_EQ(base.content.shards.size(), 1U);

    // Dropping one of the two large files leaves less than half of the shard in use, so its live chunks move into a new shard.
    files.erase("content/world/rivers.bin");
    const ReleaseBuilder::Result update = build(io::MemoryPackage("source", files), "update", "base");
    EXPECT_EQ(update.content.statistics.droppedShards, 1U);
    EXPECT_EQ(update.content.keptShards, 0U);
    ASSERT_EQ(update.content.shards.size(), 1U);
    EXPECT_LT(update.content.shards.front().fileSize, base.content.shards.front().fileSize * 2 / 3);
    EXPECT_FALSE(std::filesystem::exists(directory.getPath() / "update" / base.content.shards.front().getFileName()));
    EXPECT_EQ(open("update")->readAsset("world/terrain.bin"), files.at("content/world/terrain.bin"));
}

TEST_F(ReleaseBuilderTest, RefusesFoldersAndAppsItCannotBuild) {
    const io::MemoryPackage source("source", makeFiles());
    (void)build(source, "release");
    EXPECT_THROW((void)build(source, "release"), std::invalid_argument) << "A release never builds over another one.";

    Files broken = makeFiles();
    broken["app.json"] = test::TestFiles::bytes(R"({"name": "Release Test", "identifier": "dev.haylen.tests", "unknown": 1})");
    EXPECT_THROW((void)build(io::MemoryPackage("source", broken), "broken"), std::invalid_argument);
}

TEST_F(ReleaseBuilderTest, StopsOnADamagedEarlierRelease) {
    const io::MemoryPackage source("source", makeFiles());
    const ReleaseBuilder::Result base = build(source, "base");
    std::fstream stream(directory.getPath() / "base" / base.content.shards.front().getFileName(), std::ios::binary | std::ios::in | std::ios::out);
    stream.seekp(1000);
    stream.put('x');
    stream.close();
    try {
        (void)build(source, "update", "base");
        FAIL() << "A damaged earlier shard must stop the build.";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find("damaged or missing shard"), std::string::npos) << error.what();
    }
}

TEST_F(ReleaseBuilderTest, StopsWhenAFileChangesWhileItBuilds) {
    const ChangingPackage source(makeFiles());
    try {
        (void)build(source, "release");
        FAIL() << "A file that changes while the release builds must stop it.";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find("changed while the release was built"), std::string::npos) << error.what();
    }
}

} // namespace haylen::content
