#include <gtest/gtest.h>

#include <initializer_list>
#include <string>
#include <vector>

#include "platform/LaunchOptions.hpp"

namespace haylen::platform {

class LaunchOptionsTest : public ::testing::Test {
  protected:
    // Parses the arguments the way a process receives them, after the name of its executable.
    [[nodiscard]] static LaunchOptions parse(std::initializer_list<std::string> arguments) {
        std::vector<std::string> texts{"haylen"};
        texts.insert(texts.end(), arguments);
        std::vector<char*> pointers;
        for (std::string& text : texts) {
            pointers.push_back(text.data());
        }

        return LaunchOptions::parse(static_cast<int>(pointers.size()), pointers.data());
    }
};

TEST_F(LaunchOptionsTest, ReadsTheOptionsOfThePlayer) {
    const LaunchOptions options = parse({"app", "--dev-server", "ws://127.0.0.1:9000", "--native", "libraries", "--graphics-api", "metal", "--link", "app://open"});
    EXPECT_EQ(options.package, "app");
    EXPECT_TRUE(options.development);
    EXPECT_EQ(options.developmentServer, "ws://127.0.0.1:9000");
    EXPECT_EQ(options.nativeFolders, std::vector<std::string>{"libraries"});
    EXPECT_EQ(options.graphicsApi, "metal");
    EXPECT_EQ(options.link, "app://open");
}

// An app launched with the language of the system, as the iOS simulator and Xcode do, has no package on its command line, so the value of the option never becomes one.
TEST_F(LaunchOptionsTest, LeavesTheOptionsOfTheSystemWithTheirValues) {
    EXPECT_EQ(parse({"-AppleLanguages", "(pt-BR)"}).package, "");
    EXPECT_EQ(parse({"-AppleLanguages", "(pt-BR)", "-NSDocumentRevisionsDebugMode", "YES", "app"}).package, "app");
    EXPECT_EQ(parse({"app", "-AppleLanguages", "(es-419,en)", "--dev"}).package, "app");
    EXPECT_TRUE(parse({"app", "-AppleLanguages", "(es-419,en)", "--dev"}).development);
    EXPECT_EQ(parse({"--unknown", "app"}).package, "app");
}

// Every argument after "--" reaches the app as it is, even one that the player would read, and the player reads none of them.
TEST_F(LaunchOptionsTest, HandsTheArgumentsAfterTheSeparatorToTheApp) {
    const LaunchOptions options = parse({"app", "--dev", "--", "--test", "PLG-011", "--dev", "--"});
    EXPECT_EQ(options.package, "app");
    EXPECT_EQ(options.arguments, (std::vector<std::string>{"--test", "PLG-011", "--dev", "--"}));
    EXPECT_TRUE(parse({"app"}).arguments.empty());
    EXPECT_EQ(parse({"--", "app"}).package, "");
    EXPECT_FALSE(parse({"--", "--dev"}).development);
}

} // namespace haylen::platform
