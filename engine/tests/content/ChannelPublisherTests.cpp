#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "content/ChannelPublisher.hpp"
#include "content/PublicationLayout.hpp"
#include "content/format/ChannelDescriptor.hpp"
#include "haylen/content/Error.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "support/ReleaseFixture.hpp"
#include "support/TemporaryDirectory.hpp"
#include "support/TestFiles.hpp"

namespace haylen::content {

class ChannelPublisherTest : public ::testing::Test {
  protected:
    using Files = std::map<std::string, std::vector<std::uint8_t>>;

    ChannelPublisherTest() : signingKey(std::make_shared<const SigningKey>(test::ReleaseFixture::makeKey(101))), publisher(fixture.getKeys(), fixture.getContentKey()->getId(), signingKey) {}

    [[nodiscard]] static Files makeFiles(const std::string& level) {
        Files files;
        files.emplace("app.json", test::TestFiles::bytes(R"({"name": "Release Test", "identifier": "com.haylen.tests"})"));
        files.emplace("source/main.lua", test::TestFiles::bytes("loaded = true"));
        files.emplace("content/data/config.json", test::TestFiles::bytes(R"({"level": )" + level + "}"));
        files.emplace("content/world/terrain.bin", test::TestFiles::randomBytes(3 * 1024 * 1024, 1));
        return files;
    }

    ChannelPublisher::Publication publish(const std::string& level) const {
        return publisher.publish(io::MemoryPackage("source", makeFiles(level)), tree.getPath(), {.profile = "desktop", .appBuild = 7, .channel = "stable"});
    }

    [[nodiscard]] std::map<std::string, std::vector<std::uint8_t>> readTree() const {
        std::map<std::string, std::vector<std::uint8_t>> files;
        for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(tree.getPath())) {
            if (entry.is_regular_file()) {
                std::ifstream stream(entry.path(), std::ios::binary);
                files.emplace(entry.path().lexically_relative(tree.getPath()).generic_string(), std::vector<std::uint8_t>{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()});
            }
        }
        return files;
    }

    [[nodiscard]] ChannelDescriptor readChannel() const {
        const std::vector<std::uint8_t> bytes = readTree().at("channels/stable.hchannel");
        const VerifyingKey trusted = signingKey->getVerifyingKey();
        return ChannelDescriptor::read(bytes, std::span(&trusted, 1));
    }

    static void expectError(Error::Code code, const std::string& message, const std::function<void()>& action) {
        try {
            action();
            ADD_FAILURE() << "The action must fail with \"" << message << "\"";
        } catch (const Error& error) {
            EXPECT_EQ(error.getCode(), code);
            EXPECT_EQ(std::string(error.what()), message);
        }
    }

    test::ReleaseFixture fixture;
    test::TemporaryDirectory tree;
    std::shared_ptr<const SigningKey> signingKey;
    ChannelPublisher publisher;
};

TEST_F(ChannelPublisherTest, PublishesGenerationsThatOnlyAddFiles) {
    const ChannelPublisher::Publication first = publish("3");
    EXPECT_EQ(first.generation, 1U);
    ASSERT_EQ(first.newPacks.size(), 1U);
    const auto before = readTree();

    const ChannelPublisher::Publication second = publish("4");
    EXPECT_EQ(second.generation, 2U);
    EXPECT_EQ(second.statistics.newChunks, 1U);
    ASSERT_EQ(second.newPacks.size(), 1U);
    EXPECT_LT(second.newPacks.front().fileSize, 4096U) << "The new pack holds only the changed chunk.";

    // Every earlier file stays as it was, apart from the pointer of the channel, and no name tells what a file holds.
    const auto after = readTree();
    for (const auto& [path, bytes] : before) {
        if (path != "channels/stable.hchannel") {
            EXPECT_EQ(after.at(path), bytes) << path;
        }
    }
    EXPECT_EQ(after.size(), before.size() + 2);
    for (const auto& [path, bytes] : after) {
        EXPECT_TRUE(path.starts_with("channels/") || path.starts_with("manifests/") || path.starts_with("packs/")) << path;
        EXPECT_EQ(path.find("config"), std::string::npos) << path;
    }

    const ChannelDescriptor channel = readChannel();
    EXPECT_EQ(channel.generation, 2U);
    EXPECT_EQ(channel.manifest, second.manifest);
    EXPECT_FALSE(channel.offersUpdate(2, second.manifest));
    EXPECT_TRUE(channel.offersUpdate(1, first.manifest));
}

TEST_F(ChannelPublisherTest, LeavesTheChannelAsItWasWhenAPublishFails) {
    (void)publish("3");
    const ChannelDescriptor before = readChannel();

    // A file under the name of the next pack with other bytes stops the publish, since a published file never changes. A copy of the tree tells the name of that pack.
    const test::TemporaryDirectory copy;
    std::filesystem::copy(tree.getPath(), copy.getPath(), std::filesystem::copy_options::recursive);
    const ChannelPublisher::Publication probe = publisher.publish(io::MemoryPackage("source", makeFiles("4")), copy.getPath(), {.profile = "desktop", .appBuild = 7, .channel = "stable"});
    const std::filesystem::path taken = tree.getPath() / PublicationLayout::getPackPath(probe.newPacks.front());
    std::filesystem::create_directories(taken.parent_path());
    std::ofstream(taken) << "other bytes";

    EXPECT_THROW((void)publish("4"), std::runtime_error);
    const ChannelDescriptor after = readChannel();
    EXPECT_EQ(after.generation, before.generation);
    EXPECT_EQ(after.manifest, before.manifest);
}

TEST_F(ChannelPublisherTest, VerifiesThePacksItKeepsBeforeTheChannelMoves) {
    const ChannelPublisher::Publication first = publish("3");
    ASSERT_EQ(first.newPacks.size(), 1U);
    const ShardReference kept = first.newPacks.front();
    const std::filesystem::path keptFile = tree.getPath() / PublicationLayout::getPackPath(kept);

    // The next generation keeps the pack of the large file, so a damaged copy of it in the tree stops the publish.
    const auto middle = static_cast<std::streamoff>(kept.fileSize / 2);
    std::fstream stream(keptFile, std::ios::binary | std::ios::in | std::ios::out);
    stream.seekg(middle);
    const auto damaged = static_cast<char>(stream.get() ^ 0x01);
    stream.seekp(middle);
    stream.put(damaged);
    stream.close();
    expectError(Error::Code::CorruptHeader, "The pack \"" + kept.getFileName() + "\" of the tree is not the file its manifest signs.", [&] { (void)publish("4"); });
    EXPECT_EQ(readChannel().generation, 1U);

    // A publish that failed after it placed its new pack and manifest accepts them the next time, since they hold the same bytes, and a kept pack that is gone stops it as well.
    std::filesystem::remove(keptFile);
    expectError(Error::Code::MissingShard, "The tree has no pack \"" + kept.getFileName() + "\", which its manifest names.", [&] { (void)publish("4"); });
    EXPECT_EQ(readChannel().generation, 1U);
}

TEST_F(ChannelPublisherTest, RefusesTreesWhosePointersAndManifestsDisagree) {
    const ChannelPublisher::Publication first = publish("3");
    const ChannelPublisher::Publication second = publish("4");
    const std::filesystem::path channels = tree.getPath() / PublicationLayout::kChannelsFolder;
    const std::filesystem::path current = tree.getPath() / PublicationLayout::getManifestPath(second.manifest);

    std::filesystem::copy_file(channels / "stable.hchannel", channels / "beta.hchannel");
    expectError(Error::Code::CorruptManifest, "The pointer of the channel \"beta\" names the channel \"stable\".", [&] { (void)publisher.publish(io::MemoryPackage("source", makeFiles("5")), tree.getPath(), {.profile = "desktop", .appBuild = 7, .channel = "beta"}); });

    const std::filesystem::path saved = tree.getPath() / "saved.hmanifest";
    std::filesystem::rename(current, saved);
    std::filesystem::copy_file(tree.getPath() / PublicationLayout::getManifestPath(first.manifest), current);
    expectError(Error::Code::CorruptManifest, "The file of the manifest \"" + second.manifest.toHex() + "\" holds another manifest.", [&] { (void)publish("5"); });

    std::filesystem::remove(current);
    expectError(Error::Code::MissingManifest, "The tree has no manifest \"" + second.manifest.toHex() + "\", which a channel names.", [&] { (void)publish("5"); });

    std::filesystem::rename(saved, current);
    EXPECT_EQ(publish("5").generation, 3U);
}

TEST_F(ChannelPublisherTest, CompactsWhatNoChannelReaches) {
    (void)publish("3");
    (void)publish("4");
    const ChannelPublisher::Publication last = publish("5");
    EXPECT_THROW((void)publisher.compact(tree.getPath(), 0), std::invalid_argument);
    const test::TemporaryDirectory empty;
    EXPECT_THROW((void)publisher.compact(empty.getPath(), 1), std::invalid_argument) << "A folder without channels is no publication tree.";
    std::filesystem::create_directories(empty.getPath() / PublicationLayout::kChannelsFolder);
    const ChannelPublisher::Compaction nothing = publisher.compact(empty.getPath(), 1);
    EXPECT_EQ(nothing.manifests + nothing.packs + nothing.bytes, 0U) << "A tree that has published nothing yet has nothing to remove.";

    // Files of the channels folder that are no channel pointers stay as they are.
    std::ofstream(tree.getPath() / PublicationLayout::kChannelsFolder / "notes.txt") << "notes";
    const ChannelPublisher::Compaction compaction = publisher.compact(tree.getPath(), 1);
    EXPECT_EQ(compaction.manifests, 2U);
    EXPECT_EQ(compaction.packs, 1U) << "The pack of the second generation goes, while the first one still holds the large file of the last one.";
    EXPECT_TRUE(std::filesystem::exists(tree.getPath() / PublicationLayout::kChannelsFolder / "notes.txt"));

    // The current generation still verifies, and a new publish builds on it.
    std::set<std::string> left;
    for (const auto& [path, bytes] : readTree()) {
        left.insert(path);
    }
    EXPECT_TRUE(left.contains(PublicationLayout::getManifestPath(last.manifest)));
    EXPECT_EQ(publish("6").generation, 4U);
}

TEST_F(ChannelPublisherTest, RefusesChannelNamesThatLeaveTheChannelsFolder) {
    // A damaged file next to the channels folder shows that a refused name reads nothing, and the tree stays as it was.
    std::filesystem::create_directories(tree.getPath() / PublicationLayout::kChannelsFolder);
    std::ofstream(tree.getPath() / "x.hchannel") << "not a channel";
    EXPECT_THROW((void)publisher.publish(io::MemoryPackage("source", makeFiles("3")), tree.getPath(), {.profile = "desktop", .appBuild = 7, .channel = "../x"}), std::invalid_argument);
    EXPECT_EQ(readTree().size(), 1U);
}

TEST_F(ChannelPublisherTest, RefusesTheChannelOfAnotherApp) {
    (void)publish("3");
    Files other = makeFiles("3");
    other["app.json"] = test::TestFiles::bytes(R"({"name": "Other", "identifier": "com.haylen.other"})");
    EXPECT_THROW((void)publisher.publish(io::MemoryPackage("source", other), tree.getPath(), {.profile = "desktop", .appBuild = 7, .channel = "stable"}), std::invalid_argument);
}

} // namespace haylen::content
