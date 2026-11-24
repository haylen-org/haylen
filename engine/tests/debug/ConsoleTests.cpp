#include <gtest/gtest.h>

#include <format>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/debug/Console.hpp"

namespace haylen::debug {

class ConsoleTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // clang-format off
        gold = console.addCommand({.name = "gold", .help = "Adds gold.", .parameters = {{.kind = Console::Kind::Integer}, {.kind = Console::Kind::String, .optional = true}}, .handler = [this](const haylen::core::Json& arguments) {
            received = arguments;
            return std::format("Gold {}", arguments[0].get<std::int64_t>());
        }});
        // clang-format on
    }

    Console console;
    haylen::core::Connection gold;
    haylen::core::Json received;
};

TEST_F(ConsoleTest, RunsCommandsWithCheckedArguments) {
    const Console::Result added = console.run("  gold 50 \"the shop\"  ");
    EXPECT_TRUE(added.ok);
    EXPECT_EQ(added.output, "Gold 50");
    EXPECT_EQ(received, (haylen::core::Json{50, "the shop"}));
    EXPECT_TRUE(console.run("gold -3").ok);
    EXPECT_EQ(received, haylen::core::Json{-3});

    EXPECT_EQ(console.run("gold fifty").output, "The command \"gold\" needs an integer as its first argument, not \"fifty\". Type it as \"gold <integer> [word]\".");
    EXPECT_EQ(console.run("gold").output, "The command \"gold\" needs an integer as its first argument. Type it as \"gold <integer> [word]\".");
    EXPECT_EQ(console.run("gold 1 a b").output, "The command \"gold\" takes 2 arguments. Type it as \"gold <integer> [word]\".");
    EXPECT_EQ(console.run("silver 1").output, "There is no command \"silver\". Type \"help\" to list the commands.");
    EXPECT_EQ(console.run("gold \"1").output, "The line has a quote that is not closed.");
    EXPECT_FALSE(console.run("gold 1.5").ok);

    std::vector<std::string> said;
    // clang-format off
    const haylen::core::Connection say = console.addCommand({.name = "say", .help = "Says a text.", .parameters = {{.kind = Console::Kind::Number}, {.kind = Console::Kind::Text}}, .handler = [&said](const haylen::core::Json& arguments) {
        said.push_back(std::format("{} {}", arguments[0].get<double>(), arguments[1].get<std::string>()));
        return std::string();
    }});
    // clang-format on
    EXPECT_TRUE(console.run("say 1.5 Hello   \"there\" you").ok);
    EXPECT_EQ(said, std::vector<std::string>{"1.5 Hello   \"there\" you"});
    EXPECT_EQ(console.run("say inf words").output, "The command \"say\" needs a number as its first argument, not \"inf\". Type it as \"say <number> <text>\".");

    EXPECT_THROW((void)console.addCommand({.name = "two words", .help = {}, .parameters = {}, .handler = {}}), std::invalid_argument);
    EXPECT_THROW((void)console.addCommand({.name = "late", .help = {}, .parameters = {{.kind = Console::Kind::Integer, .optional = true}, {.kind = Console::Kind::Integer}}, .handler = {}}), std::invalid_argument);
    EXPECT_THROW((void)console.addCommand({.name = "early", .help = {}, .parameters = {{.kind = Console::Kind::Text}, {.kind = Console::Kind::Integer}}, .handler = {}}), std::invalid_argument);
}

TEST_F(ConsoleTest, FailsWithTheErrorOfAHandlerAndRemovesCommandsWithTheirConnection) {
    // clang-format off
    haylen::core::Connection broken = console.addCommand({.name = "broken", .help = "Fails.", .parameters = {}, .handler = [](const haylen::core::Json&) -> std::string {
        throw std::runtime_error("The shop is closed.");
    }});
    // clang-format on
    const Console::Result failed = console.run("broken");
    EXPECT_FALSE(failed.ok);
    EXPECT_EQ(failed.output, "The shop is closed.");

    broken.setBlocked(true);
    EXPECT_FALSE(console.run("broken").ok);
    broken.disconnect();
    EXPECT_FALSE(console.hasCommand("broken"));
    EXPECT_EQ(console.run("help").output, "gold <integer> [word]  Adds gold.\nhelp [word]            Lists the commands, or shows how to type the one it names.");
    EXPECT_EQ(console.run("help gold").output, "gold <integer> [word]\nAdds gold.");
    EXPECT_TRUE(console.removeCommand("gold"));
    EXPECT_FALSE(gold.isConnected());
}

} // namespace haylen::debug
