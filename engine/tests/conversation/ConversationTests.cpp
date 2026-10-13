#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "haylen/conversation/Conversation.hpp"
#include "haylen/conversation/ConversationState.hpp"
#include "haylen/conversation/Expression.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::conversation {

class ConversationTest : public ::testing::Test {
  protected:
    [[nodiscard]] static std::shared_ptr<const Conversation> read(const char* text) {
        return std::make_shared<const Conversation>(Conversation::parse(core::Json::parse(text)));
    }

    [[nodiscard]] static std::string refusal(const char* text) {
        try {
            (void)Conversation::parse(core::Json::parse(text));
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
    const auto lookup = [](std::string_view name) -> core::Json {
        if (name == "gold") {
            return 30;
        }
        if (name == "visits.greeting") {
            return 2;
        }
        return name == "title" ? core::Json("Sir") : core::Json();
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
    ConversationState state(read(kSmith), core::Json{{"gold", 15}}, 7);
    state.start("greeting");
    ASSERT_TRUE(state.getLine());
    EXPECT_EQ(state.getLine()->text, "smith.hello");
    EXPECT_EQ(state.getLine()->speaker, "smith");
    EXPECT_EQ(state.getLine()->tags, std::vector<std::string>{"happy"});

    std::vector<std::string> heard;
    state.advance();
    ASSERT_EQ(state.getChoices().size(), 3U);
    EXPECT_EQ(state.getLine()->text, "smith.hello") << "The line before the choices stays with them.";
    EXPECT_EQ(state.getVariables().at("metSmith"), true);
    state.deliverCommands();
    const core::Connection listening = state.commanded.connect([&heard](std::string_view name, const core::Json& arguments) { heard.push_back(std::string(name) + " " + arguments.dump()); });
    state.deliverCommands();
    EXPECT_EQ(heard, std::vector<std::string>{R"(wave {"hand":"left"})"}) << "Commands wait for the first listener.";
    EXPECT_THROW(state.advance(), std::logic_error);
    EXPECT_THROW(state.choose(3), std::out_of_range);

    state.choose(0);
    state.deliverCommands();
    EXPECT_EQ(state.getLine()->text, "smith.thanks");
    EXPECT_EQ(state.getVariables().at("gold"), 5);
    EXPECT_EQ(heard.back(), R"(giveItem {"item":"sword"})");
    state.advance();
    EXPECT_EQ(state.getLine()->text, "smith.again");
    EXPECT_EQ(state.getVisits("greeting"), 2);
    state.advance();
    ASSERT_EQ(state.getChoices().size(), 2U) << "The gold no longer allows buying.";
    EXPECT_EQ(state.getChoices()[0].text, "choice.ask");

    state.choose(0);
    EXPECT_EQ(state.getLine()->text.substr(0, 6), "rumor.");
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
        ConversationState state(rumors, core::Json(), seed);
        state.start("a");
        std::string picked;
        for (int index = 0; index < 12; ++index) {
            picked += state.getLine()->text + " ";
            state.advance();
        }
        return picked;
    };
    // clang-format on
    EXPECT_EQ(walk(3), walk(3));
    EXPECT_NE(walk(3), walk(4));

    ConversationState loop(read(R"({"nodes": {"a": {"steps": [{"jump": "b"}]}, "b": {"steps": [{"jump": "a"}]}}})"), core::Json(), 1);
    EXPECT_THROW(loop.start("a"), std::logic_error);
    EXPECT_THROW(loop.start("nowhere"), std::invalid_argument);
}

TEST_F(ConversationTest, SavesAndRestoresWhereItStood) {
    const std::shared_ptr<const Conversation> smith = read(kSmith);
    ConversationState state(smith, core::Json(), 11);
    state.start("greeting");
    state.advance();
    state.choose(1);
    const core::Json saved = state.save();

    const std::shared_ptr<ConversationState> restored = ConversationState::restore(smith, core::Json::parse(saved.dump()));
    EXPECT_EQ(restored->save(), saved);
    EXPECT_EQ(restored->getLine()->text, state.getLine()->text);
    state.advance();
    restored->advance();
    EXPECT_EQ(restored->getChoices().size(), state.getChoices().size());
    EXPECT_EQ(restored->getVisits("greeting"), 2);

    core::Json broken = saved;
    broken["node"] = "forge";
    EXPECT_THROW((void)ConversationState::restore(smith, broken), std::invalid_argument);
}

TEST(ConversationLuaTest, LoadsAssetsLocalizesLinesAndEndsListenersWithTheirOwner) {
    // clang-format off
    test::EngineFixture fixture({{"content/talk/smith.conversation", R"({
        "variables": {"gold": 30},
        "nodes": {
            "start": {
                "steps": [{"speaker": "smith", "text": "smith.hello"}, {"command": "shake"}, {"text": "Plain text."}],
                "choices": [{"text": "choice.buy", "if": "gold >= 10", "next": "start"}, {"text": "choice.leave"}]
            }
        }
    })"}});
    fixture.runLua(R"(
        local assets = require('haylen.assets')
        conversation = require('haylen.conversation')
        require('haylen.localization').add('en', {smith = {hello = 'Welcome, you have {gold} coins.'}, choice = {buy = 'Buy', leave = 'Leave'}})
        require('haylen.localization').setLanguage('en')
        talk = assets.load('talk/smith.conversation')
        state = talk:start('start', {variables = {gold = 12}, seed = 5})
        owner = {}
        heard = {}
        state:on('command', function(name) heard[#heard + 1] = name end, {owner = owner})
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return state.line.text .. ' | ' .. state.line.key .. ' | ' .. state.line.speaker"), "Welcome, you have 12 coins. | smith.hello | smith");
    EXPECT_EQ(fixture.lua("state:advance() return state.line.text .. ' ' .. #heard .. ' ' .. heard[1]"), "Plain text. 1 shake");
    EXPECT_EQ(fixture.lua("state:advance() local list = state.choices return #list .. ' ' .. list[1].text .. ' ' .. list[2].key"), "2 Buy choice.leave");
    EXPECT_EQ(fixture.lua("saved = state:save() local copy = talk:restore(saved) return #copy.choices .. ' ' .. copy.node .. ' ' .. copy:visits('start')"), "2 start 1");
    fixture.runLua("owner = nil collectgarbage() collectgarbage() state:choose(1) state:advance()");
    EXPECT_EQ(fixture.lua("return #heard"), "1");
    EXPECT_EQ(fixture.lua("state:advance() state:choose(2) return tostring(state.finished) .. ' ' .. tostring(state.line)"), "true nil");
    EXPECT_NE(fixture.lua("state:on('ended', print)").find("Unknown conversation event \"ended\". Conversations report \"command\"."), std::string::npos);
    EXPECT_NE(fixture.lua("talk:start('start', {speed = 1})").find("Unknown option \"speed\"."), std::string::npos);
    EXPECT_NE(fixture.lua("conversation.new({nodes = {a = {next = 'b'}}})").find("The node \"a\" leads to the node \"b\""), std::string::npos);
    EXPECT_EQ(fixture.lua("return table.concat(talk.texts, ',') .. ' ' .. talk.nodes[1]"), "smith.hello,Plain text.,choice.buy,choice.leave start");
}

} // namespace haylen::conversation
