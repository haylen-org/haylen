#include <gtest/gtest.h>

#include <lua.hpp>

#include <cstddef>
#include <string>

#include "support/EngineFixture.hpp"

namespace haylen::input {

class InputRecordingLuaTest : public ::testing::Test {
  protected:
    // Loads the bytes in a new app with the identifier and the fixed rate of its manifest and returns the result or the error.
    [[nodiscard]] static std::string load(const std::string& bytes, const char* manifest) {
        test::EngineFixture fixture({{"haylen.json", manifest}});
        lua_pushlstring(fixture.lua(), bytes.data(), bytes.size());
        lua_setglobal(fixture.lua(), "bytes");
        return fixture.lua("return require('haylen.input').loadRecording(bytes).steps");
    }
};

TEST_F(InputRecordingLuaTest, RecordsAndReplaysPlayersIntoACommandBuffer) {
    std::string other;
    {
        test::EngineFixture fixture({{"haylen.json", R"({"name": "Test App", "identifier": "com.haylen.tests", "fixedRate": 30})"}});
        // clang-format off
    fixture.runLua(R"(
        input = require('haylen.input')
        local m = require('haylen.math')
        buttons = {'left', 'right', 'attack'}
        random = m.random(99)
        recorder = input.newRecorder({buttons = buttons, players = 2, data = random:state()})
        function newCommands()
            return input.newCommandBuffer({buttons = buttons, lifetime = 5, directions = {back = 'left', forward = 'right'},
                commands = {{name = 'run', steps = {'forward', 'forward'}, maxTicks = 6, priority = 1}}})
        end
        live = newCommands()
        typed = {}
        local pattern = {2, 0, 2, 0, 0, 4, 0, 1, 1, 0}
        for step, mask in ipairs(pattern) do
            recorder:write(1, mask)
            recorder:write(2, step % 8)
            live:update(mask)
            typed[#typed + 1] = tostring(live:peek()) .. random:integer(1, 100)
            recorder:advance(step * 7)
        end
    )");
    // clang-format on
    // clang-format off
    EXPECT_EQ(fixture.lua(R"(
        local recording = input.loadRecording(recorder:bytes())
        local playback = input.newPlayback(recording)
        local replayRandom = require('haylen.math').random(1)
        replayRandom:setState(playback.data)
        local replayed = newCommands()
        local same = recording.steps == 10 and recording.players == 2 and recording.buttons[3] == 'attack'
        local index = 0
        while not playback.finished do
            if index == recording.steps then error('The playback goes on past the steps of its recording.') end
            index = index + 1
            replayed:update(playback:mask(1))
            same = same and tostring(replayed:peek()) .. replayRandom:integer(1, 100) == typed[index] and playback:mask(2) == index % 8 and playback:hash() == index * 7
            playback:advance()
        end
        playback:seek(1)
        return tostring(same) .. ' ' .. index .. ' ' .. playback.step .. ' ' .. playback.steps
    )"), "true 10 1 10");
        // clang-format on

        EXPECT_NE(fixture.lua("input.loadRecording('nonsense')").find("The input recording is malformed: "), std::string::npos);
        EXPECT_NE(fixture.lua("recorder:write(3, 1)").find("The input recording has players from 1 to 2."), std::string::npos);
        EXPECT_NE(fixture.lua("recorder:write(1, 1 << 31)").find("The mask of a step presses a button that the input recording does not have."), std::string::npos);
        EXPECT_NE(fixture.lua("input.newRecorder({buttons = {}})").find("An input recording needs from 1 to 32 buttons."), std::string::npos);
        EXPECT_NE(fixture.lua("input.newRecorder({buttons = {'a'}, size = 1})").find("Unknown option \"size\"."), std::string::npos);
        EXPECT_NE(fixture.lua("input.newPlayback(recorder):seek(11)").find("The playback has steps from 0 to 10."), std::string::npos);
        EXPECT_NE(fixture.lua("random:setState('short')").find("A random state needs 32 bytes."), std::string::npos);
        EXPECT_NE(fixture.lua("random:setState(string.rep('\\0', 32))").find("A random state cannot be all zeros."), std::string::npos);

        fixture.runLua("saved = recorder:bytes()");
        lua_getglobal(fixture.lua(), "saved");
        std::size_t size = 0;
        const char* data = lua_tolstring(fixture.lua(), -1, &size);
        other.assign(data, size);
        lua_pop(fixture.lua(), 1);
    }
    EXPECT_EQ(load(other, R"({"name": "Test App", "identifier": "com.haylen.tests", "fixedRate": 30})"), "10");
    EXPECT_NE(load(other, R"({"name": "Other", "identifier": "com.haylen.other", "fixedRate": 30})").find("The recording belongs to another app."), std::string::npos);
    EXPECT_NE(load(other, R"({"name": "Test App", "identifier": "com.haylen.tests", "fixedRate": 60})").find("The recording was made at 30"), std::string::npos);
}

} // namespace haylen::input
