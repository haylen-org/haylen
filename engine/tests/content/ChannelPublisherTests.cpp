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

#include "content/ChannelPublisher.hpp"
#include "content/format/ChannelDescriptor.hpp"
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
        files.emplace("app.json", test::TestFiles::bytes(R"({"name": "Release Test", "identifier": "dev.haylen.tests"})"));
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
    const std::filesystem::path taken = tree.getPath() / ChannelPublisher::getPackPath(probe.newPacks.front());
    std::filesystem::create_directories(taken.parent_path());
    std::ofstream(taken) << "other bytes";

    EXPECT_THROW((void)publish("4"), std::runtime_error);
    const ChannelDescriptor after = readChannel();
    EXPECT_EQ(after.generation, before.generation);
    EXPECT_EQ(after.manifest, before.manifest);
}

TEST_F(ChannelPublisherTest, CompactsWhatNoChannelReaches) {
    (void)publish("3");
    (void)publish("4");
    const ChannelPublisher::Publication last = publish("5");
    EXPECT_THROW((void)publisher.compact(tree.getPath(), 0), std::invalid_argument);

    const ChannelPublisher::Compaction compaction = publisher.compact(tree.getPath(), 1);
    EXPECT_EQ(compaction.manifests, 2U);
    EXPECT_EQ(compaction.packs, 1U) << "The pack of the second generation goes, while the first one still holds the large file of the last one.";

    // The current generation still verifies, and a new publish builds on it.
    std::set<std::string> left;
    for (const auto& [path, bytes] : readTree()) {
        left.insert(path);
    }
    EXPECT_TRUE(left.contains(ChannelPublisher::getManifestPath(last.manifest).generic_string()));
    EXPECT_EQ(publish("6").generation, 4U);
}

TEST_F(ChannelPublisherTest, RefusesTheChannelOfAnotherApp) {
    (void)publish("3");
    Files other = makeFiles("3");
    other["app.json"] = test::TestFiles::bytes(R"({"name": "Other", "identifier": "dev.haylen.other"})");
    EXPECT_THROW((void)publisher.publish(io::MemoryPackage("source", other), tree.getPath(), {.profile = "desktop", .appBuild = 7, .channel = "stable"}), std::invalid_argument);
}

} // namespace haylen::content
