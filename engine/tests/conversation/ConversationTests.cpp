#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/conversation/Conversation.hpp"
#include "haylen/conversation/ConversationState.hpp"
#include "haylen/conversation/Expression.hpp"

namespace haylen::conversation {

class ConversationTest : public ::testing::Test {
  protected:
    [[nodiscard]] static std::shared_ptr<const Conversation> read(const char* text) {
        return std::make_shared<const Conversation>(Conversation::parse(haylen::core::Json::parse(text)));
    }

    // Returns the line that the conversation shows, and throws when it shows none, so a test never reads a line that is not there.
    [[nodiscard]] static const ConversationState::Line& lineOf(const ConversationState& state) {
        const std::optional<ConversationState::Line>& line = state.getLine();
        if (!line) {
            throw std::runtime_error("The conversation shows no line.");
        }

        return *line;
    }

    [[nodiscard]] static std::string refusal(const char* text) {
        try {
            (void)Conversation::parse(haylen::core::Json::parse(text));
        } catch (const std::invalid_argument& error) {
            return error.what();
        }

        return {};
    }

    // clang-format off
    static constexpr const char* kSmith = R"({
        "variables": {"gold": 30, "metSmith": false},
        "nodes": {
            "greeting": {
                "steps": [
                    {"speaker": "smith", "text": "smith.hello", "tags": ["happy"], "if": "not metSmith"},
                    {"speaker": "smith", "text": "smith.again", "if": "metSmith"},
                    {"command": "wave", "arguments": {"hand": "left"}},
                    {"set": {"metSmith": true}}
                ],
                "choices": [
                    {"text": "choice.buy", "if": "gold >= 10", "next": "buy"},
                    {"text": "choice.ask", "next": "rumor", "once": true},
                    {"text": "choice.leave"}
                ]
            },
            "buy": {
                "steps": [{"set": {"gold": "gold - 10"}}, {"command": "giveItem", "arguments": {"item": "sword"}}, {"text": "smith.thanks"}],
                "next": "greeting"
            },
            "rumor": {"steps": [{"text": ["rumor.one", "rumor.two", "rumor.three"]}], "next": "greeting"}
        }
    })";
    // clang-format on
};

TEST(ExpressionTest, EvaluatesWithTheTruthOfLua) {
    // clang-format off
    const auto lookup = [](std::string_view name) -> haylen::core::Json {
        if (name == "gold") {
            return 30;
        }

        if (name == "visits.greeting") {
            return 2;
        }

        return name == "title" ? haylen::core::Json("Sir") : haylen::core::Json();
    };
    // clang-format on
    EXPECT_EQ(Expression::parse("gold - 10 * 2").evaluate(lookup), 10);
    EXPECT_EQ(Expression::parse("(gold - 10) / 4").evaluate(lookup), 5.0);
    EXPECT_EQ(Expression::parse("-gold + 1").evaluate(lookup), -29);
    EXPECT_TRUE(Expression::parse("gold >= 30 and visits.greeting == 2").test(lookup));
    EXPECT_TRUE(Expression::parse("missing == nil and not missing").test(lookup));
    EXPECT_TRUE(Expression::parse("title == 'Sir' or order").test(lookup));
    EXPECT_FALSE(Expression::parse("title ~= \"Sir\"").test(lookup));
    EXPECT_TRUE(Expression::parse("0").test(lookup)) << "Zero is true, as in Lua.";
    EXPECT_EQ(Expression::parse("missing or 5").evaluate(lookup), 5);

    // clang-format off
    const auto refusal = [](const char* text) {
        try {
            (void)Expression::parse(text);
        } catch (const std::invalid_argument& error) {
            return std::string(error.what());
        }

        return std::string();
    };
    // clang-format on
    EXPECT_EQ(refusal("gold >"), "The expression \"gold >\" ends where a value belongs at character 7.");
    EXPECT_EQ(refusal("(gold"), "The expression \"(gold\" misses a closing parenthesis at character 6.");
    EXPECT_EQ(refusal("gold 5"), "The expression \"gold 5\" has text after its end.");
    EXPECT_EQ(refusal("'open"), "The expression \"'open\" has a string that is not closed at character 1.");
    EXPECT_THROW((void)Expression::parse("title + 1").evaluate(lookup), std::invalid_argument);
}

// Integers keep every digit and wrap around past 64 bits, and longer whole numbers are floats, as in Lua.
TEST(ExpressionTest, ReadsAndComputesIntegersAsLuaDoes) {
    const Expression::Lookup lookup = [](std::string_view) { return haylen::core::Json(); };
    EXPECT_EQ(Expression::parse("9007199254740993").evaluate(lookup), 9007199254740993);
    EXPECT_EQ(Expression::parse("9223372036854775807 + 1").evaluate(lookup), std::numeric_limits<std::int64_t>::min());
    EXPECT_EQ(Expression::parse("-(0 - 9223372036854775807 - 1)").evaluate(lookup), std::numeric_limits<std::int64_t>::min());
    EXPECT_EQ(Expression::parse("4294967296 * 4294967296").evaluate(lookup), 0);
    EXPECT_EQ(Expression::parse("99999999999999999999").evaluate(lookup), 1e20);
}

// Reading and evaluating keep their place in lists instead of recursing, so an expression of any depth stays within the smallest native stack, the one of a Windows thread.
TEST(ExpressionTest, ReadsAndEvaluatesExpressionsOfAnyDepth) {
    const Expression::Lookup lookup = [](std::string_view) { return haylen::core::Json(); };
    constexpr int kLevels = 100000;
    std::string negations;
    std::string minuses;
    std::string chain = "1";
    for (int index = 0; index < kLevels; ++index) {
        negations += "not ";
        minuses += "-";
        chain += " + 1";
    }

    EXPECT_EQ(Expression::parse(std::string(kLevels, '(') + "1" + std::string(kLevels, ')')).evaluate(lookup), 1);
    EXPECT_EQ(Expression::parse(negations + "nil").evaluate(lookup), false);
    EXPECT_EQ(Expression::parse(minuses + "1").evaluate(lookup), 1);
    EXPECT_EQ(Expression::parse(chain).evaluate(lookup), kLevels + 1);
}

// Operators bind as in Lua, a comparison takes no second comparison beside it and `not` begins only a whole condition.
TEST(ExpressionTest, BindsOperatorsAsLuaDoes) {
    const Expression::Lookup lookup = [](std::string_view) { return haylen::core::Json(); };
    EXPECT_EQ(Expression::parse("not 1 == 2").evaluate(lookup), true);
    EXPECT_EQ(Expression::parse("not nil and false").evaluate(lookup), false);
    EXPECT_EQ(Expression::parse("2 - 3 - 4 * -2 / 4").evaluate(lookup), 1.0);
    EXPECT_EQ(Expression::parse("1 < 2 and 2 < 3 or false").evaluate(lookup), true);
    EXPECT_EQ(Expression::parse("1 < (2 < 3 and 4)").evaluate(lookup), true);
    EXPECT_EQ(Expression::parse("false and 1 + 'a' or 5").evaluate(lookup), 5);

    // clang-format off
    const auto refusal = [](std::string_view text) -> std::string {
        try {
            (void)Expression::parse(text);
        } catch (const std::invalid_argument& error) {
            return error.what();
        }

        return {};
    };
    // clang-format on
    EXPECT_EQ(refusal("1 < 2 < 3"), "The expression \"1 < 2 < 3\" has text after its end.");
    EXPECT_EQ(refusal("(1 < 2 < 3)"), "The expression \"(1 < 2 < 3)\" misses a closing parenthesis at character 8.");
    EXPECT_EQ(refusal("1 < (2) < 3"), "The expression \"1 < (2) < 3\" has text after its end.");
    EXPECT_EQ(refusal("1 == not 2"), "The expression \"1 == not 2\" has \"not\" where a value belongs at character 6.");
    EXPECT_EQ(refusal("-not 2"), "The expression \"-not 2\" has \"not\" where a value belongs at character 2.");
    EXPECT_EQ(refusal("((1)"), "The expression \"((1)\" misses a closing parenthesis at character 5.");
    EXPECT_EQ(refusal("1 +"), "The expression \"1 +\" ends where a value belongs at character 4.");
}

TEST_F(ConversationTest, NamesTheNodeAndTheFieldOfAMistake) {
    EXPECT_EQ(refusal(R"({"nodes": {}})"), "A conversation is an object with \"nodes\", an object of at least one node by name.");
    EXPECT_EQ(refusal(R"({"nodes": {"a": {"next": "b"}}})"), "The node \"a\" leads to the node \"b\", which the conversation does not have.");
    EXPECT_EQ(refusal(R"({"nodes": {"a": {"steps": [{"text": "x"}, {"jump": "c"}]}}})"), "The step 2 of the node \"a\" leads to the node \"c\", which the conversation does not have.");
    EXPECT_EQ(refusal(R"({"nodes": {"a": {"steps": [{"text": "x", "colour": 1}]}}})"), "The step 1 of the node \"a\" has the unknown key \"colour\".");
    EXPECT_EQ(refusal(R"({"nodes": {"a": {"steps": [{"wait": 1}]}}})"), "The step 1 of the node \"a\" has none of \"text\", \"command\", \"set\" and \"jump\".");
    EXPECT_EQ(refusal(R"({"nodes": {"a": {"choices": [{"text": "x", "if": "gold >"}]}}})"), "The choice 1 of the node \"a\" has a condition it cannot read. The expression \"gold >\" ends where a value belongs at character 7.");
    EXPECT_EQ(read(kSmith)->getTexts(), (std::vector<std::string>{"smith.thanks", "smith.hello", "smith.again", "choice.buy", "choice.ask", "choice.leave", "rumor.one", "rumor.two", "rumor.three"}));
}

TEST_F(ConversationTest, WalksLinesConditionsChoicesAndCommands) {
    ConversationState state(read(kSmith), haylen::core::Json{{"gold", 15}}, 7);
    state.start("greeting");
    ASSERT_TRUE(state.getLine());
    EXPECT_EQ(lineOf(state).text, "smith.hello");
    EXPECT_EQ(lineOf(state).speaker, "smith");
    EXPECT_EQ(lineOf(state).tags, std::vector<std::string>{"happy"});

    std::vector<std::string> heard;
    state.advance();
    ASSERT_EQ(state.getChoices().size(), 3U);
    ASSERT_TRUE(state.getLine()) << "The line before the choices stays with them.";
    EXPECT_EQ(lineOf(state).text, "smith.hello");
    EXPECT_EQ(state.getVariables().at("metSmith"), true);
    state.deliverCommands();
    const haylen::core::Connection listening = state.commanded.connect([&heard](std::string_view name, const haylen::core::Json& arguments) { heard.push_back(std::string(name) + " " + arguments.dump()); });
    state.deliverCommands();
    EXPECT_EQ(heard, std::vector<std::string>{R"(wave {"hand":"left"})"}) << "Commands wait for the first listener.";
    EXPECT_THROW(state.advance(), std::logic_error);
    EXPECT_THROW(state.choose(3), std::out_of_range);

    state.choose(0);
    state.deliverCommands();
    EXPECT_EQ(lineOf(state).text, "smith.thanks");
    EXPECT_EQ(state.getVariables().at("gold"), 5);
    EXPECT_EQ(heard.back(), R"(giveItem {"item":"sword"})");
    state.advance();
    EXPECT_EQ(lineOf(state).text, "smith.again");
    EXPECT_EQ(state.getVisits("greeting"), 2);
    state.advance();
    ASSERT_EQ(state.getChoices().size(), 2U) << "The gold no longer allows buying.";
    EXPECT_EQ(state.getChoices()[0].text, "choice.ask");

    state.choose(0);
    EXPECT_EQ(lineOf(state).text.substr(0, 6), "rumor.");
    state.advance();
    state.advance();
    ASSERT_EQ(state.getChoices().size(), 1U) << "A choice taken once is gone.";
    state.choose(0);
    EXPECT_TRUE(state.isFinished());
    EXPECT_FALSE(state.getLine());
    EXPECT_THROW(state.advance(), std::logic_error);
}

TEST_F(ConversationTest, PicksTheSameVariantsForTheSameSeedAndStopsLoops) {
    const std::shared_ptr<const Conversation> rumors = read(R"({"nodes": {"a": {"steps": [{"text": ["one", "two", "three", "four"]}], "next": "a"}}})");
    // clang-format off
    const auto walk = [&rumors](std::uint64_t seed) {
        ConversationState state(rumors, haylen::core::Json(), seed);
        state.start("a");
        std::string picked;
        for (int index = 0; index < 12; ++index) {
            picked += lineOf(state).text + " ";
            state.advance();
        }

        return picked;
    };
    // clang-format on
    EXPECT_EQ(walk(3), walk(3));
    EXPECT_NE(walk(3), walk(4));

    ConversationState loop(read(R"({"nodes": {"a": {"steps": [{"jump": "b"}]}, "b": {"steps": [{"jump": "a"}]}}})"), haylen::core::Json(), 1);
    EXPECT_THROW(loop.start("a"), std::logic_error);
    EXPECT_THROW(loop.start("nowhere"), std::invalid_argument);
}

TEST_F(ConversationTest, StartsAgainWithoutTheChoicesAndTheLineOfTheNodeItLeft) {
    ConversationState state(read(R"({"nodes": {"ask": {"steps": [{"text": "question"}], "choices": [{"text": "yes"}, {"text": "no"}, {"text": "maybe"}]}, "pick": {"choices": [{"text": "only", "next": "ask"}]}}})"), haylen::core::Json(), 1);
    state.start("ask");
    state.advance();
    ASSERT_EQ(state.getChoices().size(), 3U);

    state.start("pick");
    EXPECT_FALSE(state.getLine());
    ASSERT_EQ(state.getChoices().size(), 1U);
    EXPECT_EQ(state.getChoices()[0].text, "only");

    state.choose(0);
    ASSERT_TRUE(state.getLine());
    EXPECT_EQ(lineOf(state).text, "question");
}

// A listener may advance the state, and the commands of the steps it runs reach the app after the ones that waited before them.
TEST_F(ConversationTest, DeliversCommandsInOrderWhenAListenerAdvances) {
    ConversationState state(read(R"({"nodes": {"a": {"steps": [{"command": "first"}, {"command": "second"}, {"text": "x"}, {"command": "third"}, {"text": "y"}]}}})"), haylen::core::Json(), 1);
    state.start("a");
    std::vector<std::string> heard;
    // clang-format off
    const haylen::core::Connection listening = state.commanded.connect([&heard, &state](std::string_view name, const haylen::core::Json&) {
        heard.emplace_back(name);

        if (name == "first") {
            state.advance();
            state.deliverCommands();
        }
    });
    // clang-format on
    state.deliverCommands();

    EXPECT_EQ(heard, (std::vector<std::string>{"first", "second", "third"}));
    EXPECT_EQ(lineOf(state).text, "y");
}

TEST_F(ConversationTest, SavesAndRestoresWhereItStood) {
    const std::shared_ptr<const Conversation> smith = read(kSmith);
    ConversationState state(smith, haylen::core::Json(), 11);
    state.start("greeting");
    state.advance();
    state.choose(1);
    const haylen::core::Json saved = state.save();

    const std::shared_ptr<ConversationState> restored = ConversationState::restore(smith, haylen::core::Json::parse(saved.dump()));
    EXPECT_EQ(restored->save(), saved);
    EXPECT_EQ(lineOf(*restored).text, lineOf(state).text);
    state.advance();
    restored->advance();
    EXPECT_EQ(restored->getChoices().size(), state.getChoices().size());
    EXPECT_EQ(restored->getVisits("greeting"), 2);

    haylen::core::Json broken = saved;
    broken["node"] = "forge";
    EXPECT_THROW((void)ConversationState::restore(smith, broken), std::invalid_argument);
}

} // namespace haylen::conversation
