#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "content/Error.hpp"
#include "content/ReleaseInspector.hpp"
#include "content/ReleasePackage.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "support/ReleaseFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::content {

class ReleaseInspectorTest : public ::testing::Test {
  protected:
    using Files = std::map<std::string, std::vector<std::uint8_t>>;

    static constexpr std::size_t kMegabyte = 1024 * 1024;

    [[nodiscard]] static Files makeFiles() {
        Files files;
        const auto text = [&files](const std::string& path, const std::string& content) { files.emplace(path, test::TestFiles::bytes(content)); };
        text("app.json", R"({"name": "Release Test", "identifier": "dev.haylen.tests"})");
        text("source/main.lua", "loaded = true");
        text("content/data/config.json", R"({"level": 3})");
        text("content/data/removed.json", R"({"removed": true})");
        files.emplace("content/world/terrain.bin", test::TestFiles::randomBytes(6 * kMegabyte, 1));
        files.emplace("content/world/copy.bin", files.at("content/world/terrain.bin"));
        return files;
    }

    [[nodiscard]] ReleaseInspector makeInspector() const {
        return {release.getKeys(), {release.getSigningKey().getVerifyingKey()}};
    }

    static void expectCode(Error::Code code, const std::function<void()>& action) {
        try {
            action();
            ADD_FAILURE() << "The action must fail.";
        } catch (const Error& error) {
            EXPECT_EQ(error.getCode(), code) << error.what();
        }
    }

    test::ReleaseFixture release;
};

TEST_F(ReleaseInspectorTest, ShowsTheFilesChunksAndShardsOfARelease) {
    const Files files = makeFiles();
    release.build(io::MemoryPackage("source", files));
    const ReleaseInspector::Report report = makeInspector().inspect(release.getFolder());

    EXPECT_EQ(report.app.envelope.domain, Manifest::Domain::App);
    EXPECT_EQ(report.app.envelope.profile, test::ReleaseFixture::kProfile);
    ASSERT_EQ(report.app.files.size(), 2U);
    EXPECT_EQ(report.app.files.front().path, "app.json");
    ASSERT_EQ(report.content.files.size(), 4U);

    // The copy of the large file stores nothing, which the totals show as the saving of deduplication.
    const std::uint64_t large = files.at("content/world/terrain.bin").size();
    EXPECT_EQ(ReleaseInspector::getFileBytes(report.content) - ReleaseInspector::getChunkBytes(report.content), large);
    EXPECT_LT(ReleaseInspector::getStoredBytes(report.content), 2 * large);
    const ReleaseInspector::File& terrain = report.content.files.back();
    EXPECT_EQ(terrain.path, "content/world/terrain.bin");
    EXPECT_GT(terrain.chunks.size(), 1U);
    EXPECT_EQ(terrain.chunks.back().offset + terrain.chunks.back().plainSize, large);
}

TEST_F(ReleaseInspectorTest, ComparesTwoReleases) {
    Files files = makeFiles();
    release.build(io::MemoryPackage("source", files));
    const ReleaseInspector::Report before = makeInspector().inspect(release.getFolder());

    files["content/data/config.json"] = test::TestFiles::bytes(R"({"level": 4})");
    files.erase("content/data/removed.json");
    files["content/data/added.json"] = test::TestFiles::bytes(R"({"added": true})");
    release.build(io::MemoryPackage("source", files));
    const ReleaseInspector::Report after = makeInspector().inspect(release.getFolder());

    const ReleaseInspector::Difference content = ReleaseInspector::compare(before.content, after.content);
    EXPECT_EQ(content.newChunks, 2U);
    EXPECT_EQ(content.removedChunks, 2U);
    EXPECT_EQ(content.addedFiles, std::vector<std::string>{"content/data/added.json"});
    EXPECT_EQ(content.changedFiles, std::vector<std::string>{"content/data/config.json"});
    EXPECT_EQ(content.removedFiles, std::vector<std::string>{"content/data/removed.json"});
    ASSERT_EQ(content.newShards.size(), 1U);
    EXPECT_LT(content.downloadBytes, 1024U) << "An app downloads only the records of the new chunks.";

    const ReleaseInspector::Difference app = ReleaseInspector::compare(before.app, after.app);
    EXPECT_EQ(app.newChunks, 0U);
    EXPECT_TRUE(app.newShards.empty());
}

TEST_F(ReleaseInspectorTest, VerifiesEveryByteOfARelease) {
    release.build(io::MemoryPackage("source", makeFiles()));
    const ReleaseInspector inspector = makeInspector();
    const ReleaseInspector::Verification verification = inspector.verify(release.getFolder());
    EXPECT_EQ(verification.files, 6U);
    EXPECT_EQ(verification.shards, 2U);

    // A file the manifests do not name never belongs in a release.
    const std::filesystem::path stray = release.getFolder() / "main.lua";
    std::ofstream(stray) << "loaded = true";
    EXPECT_THROW((void)inspector.verify(release.getFolder()), std::runtime_error);
    std::filesystem::remove(stray);

    const ShardReference shard = inspector.inspect(release.getFolder()).content.envelope.shards.front();
    std::fstream stream(release.getFolder() / shard.getFileName(), std::ios::binary | std::ios::in | std::ios::out);
    stream.seekp(static_cast<std::streamoff>(shard.fileSize / 2));
    stream.put('x');
    stream.close();
    expectCode(Error::Code::CorruptHeader, [&] { (void)inspector.verify(release.getFolder()); });

    std::filesystem::remove(release.getFolder() / shard.getFileName());
    expectCode(Error::Code::MissingShard, [&] { (void)inspector.verify(release.getFolder()); });
}

} // namespace haylen::content
