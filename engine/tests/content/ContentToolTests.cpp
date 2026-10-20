#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "content/ContentTool.hpp"
#include "content/crypto/Digest.hpp"
#include "support/PrivateFiles.hpp"
#include "support/TemporaryDirectory.hpp"
#include "support/TestFiles.hpp"

namespace haylen::content {

class ContentToolTest : public ::testing::Test {
  protected:
    ContentToolTest() {
        app.write("app.json", R"({"name": "Tool Test", "identifier": "com.haylen.tests", "version": "1.0.0"})");
        app.write("source/main.lua", "loaded = true");
        app.write("content/data/config.json", R"({"level": 3})");
    }

    // Runs a command line and returns its exit code, keeping what it printed.
    int run(std::vector<std::string> arguments) {
        output.str("");
        errors.str("");
        return ContentTool(output, errors).run(arguments);
    }

    [[nodiscard]] std::string getPath(const std::string& name) const {
        return (work.getPath() / name).generic_string();
    }

    // The hexadecimal digits of every secret of the key folder, which no output may show.
    [[nodiscard]] std::vector<std::string> readSecrets() const {
        std::vector<std::string> secrets;
        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(getPath("keys"))) {
            if (entry.path().extension() == ".key") {
                std::ifstream stream(entry.path(), std::ios::binary);
                const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
                secrets.push_back(Digest(std::span(bytes).first<Digest::kSize>()).toHex());
            }
        }
        return secrets;
    }

    void expectNoSecret() const {
        for (const std::string& secret : readSecrets()) {
            EXPECT_EQ(output.str().find(secret), std::string::npos);
            EXPECT_EQ(errors.str().find(secret), std::string::npos);
        }
    }

    test::TemporaryDirectory app;
    test::TemporaryDirectory work;
    std::ostringstream output;
    std::ostringstream errors;
};

TEST_F(ContentToolTest, BuildsInspectsAndPublishesReleases) {
    ASSERT_EQ(run({"keys", "create", getPath("keys"), "--identifier", "com.haylen.tests"}), 0) << errors.str();
    EXPECT_NE(output.str().find("Created the keys of \"com.haylen.tests\""), std::string::npos) << output.str();
    expectNoSecret();

    const std::vector<std::string> build = {"build", app.getPath().generic_string(), "--keys", getPath("keys"), "--profile", "desktop", "--build", "7", "--cache", getPath("cache")};
    std::vector<std::string> first = build;
    first.insert(first.end(), {"--output", getPath("first")});
    ASSERT_EQ(run(first), 0) << errors.str();
    EXPECT_NE(output.str().find("Built the release of \"com.haylen.tests\" for the profile \"desktop\""), std::string::npos) << output.str();

    app.write("content/data/config.json", R"({"level": 4})");
    std::vector<std::string> second = build;
    second.insert(second.end(), {"--output", getPath("second"), "--previous", getPath("first")});
    ASSERT_EQ(run(second), 0) << errors.str();

    ASSERT_EQ(run({"verify", getPath("second"), "--keys", getPath("keys")}), 0) << errors.str();
    EXPECT_NE(output.str().find("3 files"), std::string::npos) << output.str();
    ASSERT_EQ(run({"stage", getPath("second"), "--keys", getPath("keys"), "--output", getPath("shipped"), "--deferred", getPath("deferred")}), 0) << errors.str();
    EXPECT_NE(output.str().find("files ship with the app"), std::string::npos) << output.str();
    EXPECT_TRUE(std::filesystem::exists(work.getPath() / "shipped" / "content.hmanifest"));
    ASSERT_EQ(run({"inspect", getPath("second"), "--keys", getPath("keys")}), 0) << errors.str();
    EXPECT_NE(output.str().find("File \"content/data/config.json\" holds 12 bytes of data, required"), std::string::npos) << output.str();
    EXPECT_EQ(output.str().find("Chunk at 0"), std::string::npos) << "Only \"--chunks\" lists the chunks.";
    ASSERT_EQ(run({"inspect", getPath("second"), "--keys", getPath("keys"), "--chunks"}), 0) << errors.str();
    EXPECT_NE(output.str().find("File \"source/main.lua\" holds"), std::string::npos) << output.str();
    EXPECT_NE(output.str().find("bytes of Lua bytecode, required"), std::string::npos) << output.str();
    EXPECT_NE(output.str().find("Its Lua bytecode has the ABI \"lua-"), std::string::npos) << output.str();
    EXPECT_NE(output.str().find("Chunk at 0"), std::string::npos) << output.str();
    ASSERT_EQ(run({"diff", getPath("first"), getPath("second"), "--keys", getPath("keys")}), 0) << errors.str();
    EXPECT_NE(output.str().find("Changed \"content/data/config.json\""), std::string::npos) << output.str();

    ASSERT_EQ(run({"publish", app.getPath().generic_string(), "--keys", getPath("keys"), "--profile", "desktop", "--build", "7", "--channel", "stable", "--output", getPath("tree")}), 0) << errors.str();
    EXPECT_NE(output.str().find("Published generation 1 of the channel \"stable\""), std::string::npos) << output.str();
    ASSERT_EQ(run({"compact", getPath("tree"), "--keys", getPath("keys"), "--keep", "1"}), 0) << errors.str();
    ASSERT_EQ(run({"compact", getPath("tree"), "--keys", getPath("keys")}), 0) << errors.str();
    EXPECT_NE(output.str().find("within its last 2 generations"), std::string::npos) << output.str();
    ASSERT_EQ(run({"bootstrap", "--keys", getPath("keys"), "--profile", "desktop", "--build", "7", "--output", getPath("bootstrap/HaylenBootstrap.cpp")}), 0) << errors.str();
    std::ifstream bootstrap(work.getPath() / "bootstrap" / "HaylenBootstrap.cpp");
    EXPECT_NE(std::string(std::istreambuf_iterator<char>(bootstrap), std::istreambuf_iterator<char>()).find("haylen::content::Bootstrap::install"), std::string::npos);
    test::PrivateFiles::expectOwnerOnly(work.getPath() / "bootstrap" / "HaylenBootstrap.cpp");
    ASSERT_EQ(run({"keys", "rotate", getPath("keys")}), 0) << errors.str();
    ASSERT_EQ(run({"keys", "show", getPath("keys")}), 0) << errors.str();
    EXPECT_NE(output.str().find("which encrypts new content"), std::string::npos) << output.str();
    expectNoSecret();
}

TEST_F(ContentToolTest, ExplainsWhatACommandLacks) {
    EXPECT_EQ(run({}), 1);
    EXPECT_NE(errors.str().find("Error: Name a command"), std::string::npos) << errors.str();
    EXPECT_EQ(run({"unknown"}), 1);
    EXPECT_NE(errors.str().find("The command \"unknown\" is unknown."), std::string::npos) << errors.str();
    EXPECT_EQ(run({"verify", getPath("missing")}), 1);
    EXPECT_NE(errors.str().find("needs the option \"--keys\""), std::string::npos) << errors.str();
    EXPECT_EQ(run({"keys", "show", getPath("keys"), "--surprise", "1"}), 1);
    EXPECT_NE(errors.str().find("takes no option \"--surprise\""), std::string::npos) << errors.str();
    EXPECT_EQ(run({"keys"}), 1);
    EXPECT_EQ(errors.str(), "Error: The command needs the action, \"create\", \"rotate\" or \"show\".\n");
    EXPECT_EQ(run({"keys", "remove", getPath("keys")}), 1);
    EXPECT_EQ(errors.str(), "Error: The keys command takes \"create\", \"rotate\" or \"show\", not \"remove\".\n");
    EXPECT_EQ(run({"keys", "create", getPath("keys"), "--identifier", "com.haylen.tests", "--identifier", "com.haylen.other"}), 1);
    EXPECT_EQ(errors.str(), "Error: The option \"--identifier\" is given twice.\n");
    EXPECT_FALSE(std::filesystem::exists(getPath("keys")));

    ASSERT_EQ(run({"keys", "create", getPath("keys"), "--identifier", "com.haylen.other"}), 0) << errors.str();
    EXPECT_EQ(run({"keys", "show", getPath("keys"), "extra"}), 1);
    EXPECT_EQ(errors.str(), "Error: The command takes no argument \"extra\".\n");
    EXPECT_EQ(run({"keys", "show", getPath("keys"), "--chunks"}), 1);
    EXPECT_EQ(errors.str(), "Error: The command takes no option \"--chunks\", or it needs a value.\n");
    EXPECT_EQ(run({"build", app.getPath().generic_string(), "--keys", getPath("keys"), "--profile", "desktop", "--output", getPath("release")}), 1);
    EXPECT_EQ(errors.str(), "Error: The command needs the option \"--build\".\n");
    EXPECT_EQ(run({"build", app.getPath().generic_string(), "--keys", getPath("keys"), "--profile", "desktop", "--build", "7", "--output", getPath("release")}), 1);
    EXPECT_NE(errors.str().find("cannot build with the keys of \"com.haylen.other\""), std::string::npos) << errors.str();
    EXPECT_EQ(run({"build", app.getPath().generic_string(), "--keys", getPath("keys"), "--profile", "desktop", "--build", "seven", "--output", getPath("release")}), 1);
    EXPECT_NE(errors.str().find("takes a whole number"), std::string::npos) << errors.str();
    EXPECT_EQ(run({"publish", app.getPath().generic_string(), "--keys", getPath("keys"), "--profile", "desktop", "--build", "7", "--channel", "stable", "--output", getPath("tree")}), 1);
    EXPECT_EQ(errors.str(), "Error: The app cannot publish with the keys of \"com.haylen.other\".\n");
    EXPECT_FALSE(std::filesystem::exists(getPath("tree")));
}

TEST_F(ContentToolTest, BuildsShardsOfTheSizeThatItsTargetNames) {
    constexpr std::size_t kFileSize = 256 * 1024;
    for (std::uint64_t index = 0; index < 4; ++index) {
        const std::vector<std::uint8_t> bytes = test::TestFiles::randomBytes(kFileSize, index + 1);
        app.write("content/world/part-" + std::to_string(index) + ".bin", std::string(bytes.begin(), bytes.end()));
    }
    ASSERT_EQ(run({"keys", "create", getPath("keys"), "--identifier", "com.haylen.tests"}), 0) << errors.str();
    // clang-format off
    const auto build = [this](const std::string& name, std::vector<std::string> extra) {
        std::vector<std::string> arguments = {"build", app.getPath().generic_string(), "--keys", getPath("keys"), "--profile", "android", "--build", "7", "--output", getPath(name)};
        arguments.insert(arguments.end(), extra.begin(), extra.end());
        return run(arguments);
    };
    const auto largestShard = [this](const std::string& name) {
        std::uintmax_t largest = 0;
        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(getPath(name))) {
            largest = entry.path().extension() == ".hpak" ? std::max(largest, entry.file_size()) : largest;
        }
        return largest;
    };
    // clang-format on

    ASSERT_EQ(build("whole", {}), 0) << errors.str();
    EXPECT_GT(largestShard("whole"), 4 * kFileSize) << "The default target keeps the content in one shard.";
    ASSERT_EQ(build("split", {"--shard-target", std::to_string(kFileSize)}), 0) << errors.str();
    EXPECT_LE(largestShard("split"), kFileSize + kFileSize / 4) << "A shard passes its target by at most one record.";
    ASSERT_EQ(run({"verify", getPath("split"), "--keys", getPath("keys")}), 0) << errors.str();

    EXPECT_EQ(build("empty", {"--shard-target", "0"}), 1);
    EXPECT_EQ(errors.str(), "Error: The option \"--shard-target\" takes a size above zero bytes.\n");
}

} // namespace haylen::content
