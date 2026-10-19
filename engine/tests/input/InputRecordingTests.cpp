#include <gtest/gtest.h>

#include <lua.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/input/InputPlayback.hpp"
#include "haylen/input/InputRecording.hpp"
#include "haylen/math/Random.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::input {

class InputRecordingTest : public ::testing::Test {
  protected:
    [[nodiscard]] static InputRecording makeRecording() {
        return InputRecording({.buttons = {"left", "right", "jump"}, .players = 2, .identifier = "com.haylen.tests", .fixedRate = 30.0, .data = "seed"});
    }

    [[nodiscard]] static std::string malformed(const std::vector<std::byte>& bytes) {
        try {
            (void)InputRecording::fromBytes(bytes);
        } catch (const std::invalid_argument& error) {
            return error.what();
        }
        return {};
    }
};

TEST_F(InputRecordingTest, KeepsEveryStepThroughItsBytes) {
    InputRecording recording = makeRecording();
    for (std::uint32_t step = 0; step < 10000; ++step) {
        recording.write(0, step < 5000 ? 0b010U : 0b101U);
        recording.write(1, step % 7 == 0 ? 0b100U : 0U);
        recording.advance(step * 3U);
    }
    const std::vector<std::byte> bytes = recording.toBytes();
    const InputRecording loaded = InputRecording::fromBytes(bytes);
    EXPECT_EQ(loaded.getStepCount(), 10000U);
    EXPECT_EQ(loaded.getHeader().buttons, recording.getHeader().buttons);
    EXPECT_EQ(loaded.getHeader().players, 2U);
    EXPECT_EQ(loaded.getHeader().identifier, "com.haylen.tests");
    EXPECT_EQ(loaded.getHeader().fixedRate, 30.0);
    EXPECT_EQ(loaded.getHeader().data, "seed");
    for (std::uint32_t step = 0; step < 10000; step += 13) {
        EXPECT_EQ(loaded.getMask(step, 0), recording.getMask(step, 0));
        EXPECT_EQ(loaded.getMask(step, 1), recording.getMask(step, 1));
        EXPECT_EQ(loaded.getHash(step), step * 3U);
    }
    EXPECT_EQ(loaded.toBytes(), bytes);

    // A button held for a long time costs one run, however many steps it lasts.
    InputRecording held({.buttons = {"jump"}});
    for (int step = 0; step < 100000; ++step) {
        held.write(0, 1);
        held.advance();
    }
    EXPECT_LT(held.toBytes().size(), 64U);
    EXPECT_FALSE(held.getHash(0).has_value());
    EXPECT_THROW(held.write(1, 0), std::out_of_range);
    EXPECT_THROW((void)held.getMask(100000, 0), std::out_of_range);
}

TEST_F(InputRecordingTest, PlaysTheStepsBackAndSeeks) {
    auto recording = std::make_shared<InputRecording>(makeRecording());
    for (std::uint32_t step = 0; step < 4; ++step) {
        recording->write(0, step);
        recording->advance();
    }
    InputPlayback playback(recording);
    std::vector<std::uint32_t> played;
    while (!playback.isFinished()) {
        played.push_back(playback.getMask(0));
        playback.advance();
    }
    EXPECT_EQ(played, (std::vector<std::uint32_t>{0, 1, 2, 3}));
    EXPECT_EQ(playback.getMask(0), 0U);
    playback.seek(2);
    EXPECT_EQ(playback.getMask(0), 2U);
    EXPECT_THROW(playback.seek(5), std::out_of_range);
    EXPECT_THROW((void)playback.getMask(2), std::out_of_range);
}

TEST_F(InputRecordingTest, RefusesMalformedBytes) {
    InputRecording recording = makeRecording();
    recording.write(0, 3);
    recording.advance();
    std::vector<std::byte> bytes = recording.toBytes();

    EXPECT_EQ(malformed({}), "The input recording is malformed: it ends before its last number.");
    EXPECT_EQ(malformed({std::byte{1}}), "The input recording is malformed: it does not start like one.");
    std::vector<std::byte> truncated(bytes.begin(), bytes.end() - 1);
    EXPECT_EQ(malformed(truncated), "The input recording is malformed: it ends before its last number.");
    std::vector<std::byte> trailing = bytes;
    trailing.push_back(std::byte{0});
    EXPECT_EQ(malformed(trailing), "The input recording is malformed: bytes follow its last step.");
    std::vector<std::byte> endless(bytes.begin(), bytes.begin() + 5);
    endless.insert(endless.end(), 11, std::byte{0xFF});
    EXPECT_NE(malformed(endless).find("The input recording is malformed: "), std::string::npos);

    // A tiny file that claims the most steps in one run decodes without holding a mask per step.
    InputRecording single({.buttons = {"jump"}});
    single.write(0, 1);
    single.advance();
    std::vector<std::byte> claimed = single.toBytes();
    const std::size_t header = claimed.size() - 4;
    claimed.resize(header);
    for (const std::uint64_t value : {std::uint64_t{InputRecording::kMaxSteps}, std::uint64_t{0}, std::uint64_t{InputRecording::kMaxSteps}, std::uint64_t{1}}) {
        std::uint64_t rest = value;
        do {
            const auto low = static_cast<std::uint8_t>(rest & 0x7FU);
            rest >>= 7U;
            claimed.push_back(static_cast<std::byte>(rest != 0 ? low | 0x80U : low));
        } while (rest != 0);
    }
    const InputRecording huge = InputRecording::fromBytes(claimed);
    EXPECT_EQ(huge.getStepCount(), InputRecording::kMaxSteps);
    EXPECT_EQ(huge.getMask(InputRecording::kMaxSteps - 1, 0), 1U);

    EXPECT_THROW(InputRecording({.buttons = {}}), std::invalid_argument);
    EXPECT_THROW(InputRecording({.buttons = {"a"}, .players = 17}), std::invalid_argument);
}

TEST(RandomStateTest, RepeatsTheSequenceFromARestoredState) {
    math::Random random(1234);
    (void)random.nextU64();
    const auto saved = random.getState();
    const std::uint64_t first = random.nextU64();
    const std::uint64_t second = random.nextU64();
    random.setState(saved);
    EXPECT_EQ(random.nextU64(), first);
    EXPECT_EQ(random.nextU64(), second);
    EXPECT_THROW(random.setState({}), std::invalid_argument);
}

class InputRecordingLuaTest : public ::testing::Test {
  protected:
    // Loads the bytes in a new app with the identifier and the fixed rate of its manifest and returns the result or the error.
    [[nodiscard]] static std::string load(const std::string& bytes, const char* manifest) {
        test::EngineFixture fixture({{"app.json", manifest}});
        lua_pushlstring(fixture.lua(), bytes.data(), bytes.size());
        lua_setglobal(fixture.lua(), "bytes");
        return fixture.lua("return require('haylen.input').loadRecording(bytes).steps");
    }
};

TEST_F(InputRecordingLuaTest, RecordsAndReplaysPlayersIntoACommandBuffer) {
    std::string other;
    {
        test::EngineFixture fixture({{"app.json", R"({"name": "Test App", "identifier": "com.haylen.tests", "fixedRate": 30})"}});
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
            recorder:write(2, step)
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
            index = index + 1
            replayed:update(playback:mask(1))
            same = same and tostring(replayed:peek()) .. replayRandom:integer(1, 100) == typed[index] and playback:mask(2) == index and playback:hash() == index * 7
            playback:advance()
        end
        playback:seek(1)
        return tostring(same) .. ' ' .. index .. ' ' .. playback.step .. ' ' .. playback.steps
    )"), "true 10 1 10");
        // clang-format on

        EXPECT_NE(fixture.lua("input.loadRecording('nonsense')").find("The input recording is malformed: "), std::string::npos);
        EXPECT_NE(fixture.lua("recorder:write(3, 1)").find("The input recording has players from 1 to 2."), std::string::npos);
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
