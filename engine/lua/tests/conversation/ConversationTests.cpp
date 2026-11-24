#include <gtest/gtest.h>

#include <string>

#include "support/LuaFixture.hpp"

namespace haylen::lua::conversation {

TEST(ConversationLuaTest, LoadsAssetsLocalizesLinesAndEndsListenersWithTheirOwner) {
    // clang-format off
    test::LuaFixture fixture({{"content/talk/smith.conversation", R"({
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

TEST(ConversationLuaTest, ListenersWithoutAnOwnerEndWithTheirStateAndNeverKeepItAlive) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        talk = require('haylen.conversation').new({nodes = {start = {steps = {{text = 'one'}, {command = 'wave'}, {text = 'two'}}}}})
        heard = 0
        walk = talk:start('start')
        walk:on('command', function() heard = heard + 1 return walk end)
        walk:advance()
        collected = setmetatable({}, {__mode = 'v'})
        do
            local other = talk:start('start')
            other:on('command', function() return other end)
            collected[1] = other
        end
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return heard"), "1");
    EXPECT_EQ(fixture.lua("collectgarbage() collectgarbage() return tostring(collected[1])"), "nil");
    EXPECT_NE(fixture.lua("talk:start('start', {seed = 1.5})").find("The option \"seed\" of \"start\" is invalid: number has no integer representation."), std::string::npos);
}

TEST(ConversationLuaTest, FillsThePlaceholdersOfTextsThatAreNotKeys) {
    // clang-format off
    test::LuaFixture fixture({{"content/talk/smith.conversation", R"({
        "variables": {"swords": 0},
        "nodes": {
            "buy": {
                "steps": [{"set": {"swords": "swords + 1"}}, {"text": "That makes {swords} so far, {{and}} {missing}."}],
                "choices": [{"text": "Buy sword {swords}."}]
            }
        }
    })"}});
    // clang-format on
    fixture.runLua("state = require('haylen.assets').load('talk/smith.conversation'):start('buy', {seed = 1})");
    EXPECT_EQ(fixture.lua("return state.line.text .. ' | ' .. state.line.key"), "That makes 1 so far, {and} {missing}. | That makes {swords} so far, {{and}} {missing}.");
    EXPECT_EQ(fixture.lua("state:advance() return state.choices[1].text"), "Buy sword 1.");
}

} // namespace haylen::lua::conversation
