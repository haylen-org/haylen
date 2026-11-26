#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "audio/MixerFixture.hpp"
#include "support/LuaFixture.hpp"

namespace haylen::lua::audio {

class AudioTapLuaTest : public haylen::audio::MixerFixture {
  protected:
    // An engine whose mixer keeps time without sound, as the headless host does, with a hit sound that holds 0.5 for a tenth of a second.
    [[nodiscard]] static std::unique_ptr<test::LuaFixture> makeEngine() {
        const std::vector<std::uint8_t> hit = test::TestFiles::wav(kRate, 1, makeSignal(4800, 1, 0.5F));
        auto fixture = std::make_unique<test::LuaFixture>(std::map<std::string, std::string>{{"content/sfx/hit.wav", std::string(hit.begin(), hit.end())}});
        fixture->runLua("audio = require('haylen.audio') collections = require('haylen.collections') hit = require('haylen.assets').load('sfx/hit.wav')");
        return fixture;
    }
};

// The app reads what arrived since its last read in one call, as a string of float samples that a sound plays again, or into a float buffer.
TEST_F(AudioTapLuaTest, ReadsTheMixInBatches) {
    const auto fixture = makeEngine();
    fixture->runLua("tap = audio.tapMaster({seconds = 0.5}) audio.play(hit)");
    fixture->frames(3);
    EXPECT_EQ(fixture->lua("return table.concat({tap.sampleRate, tap.channels, tap.available, tap.dropped, tostring(tap.closed)}, ' ')"), "48000 2 2400 0 false");

    // clang-format off
    const std::string read = fixture->lua(R"(
        local samples = tap:read()
        local loudest = 0
        for offset = 1, #samples, 4 do
            loudest = math.max(loudest, (string.unpack('f', samples, offset)))
        end
        local again = audio.newSound(samples, {format = 'float32', sampleRate = tap.sampleRate, channels = tap.channels})
        return table.concat({#samples, string.format('%.2f', loudest), again.frameCount, tap.available}, ' ')
    )");
    // clang-format on
    EXPECT_EQ(read, "19200 0.50 2400 0");

    fixture->frames(1);
    EXPECT_EQ(fixture->lua("local buffer = collections.newFloatBuffer(1000) return table.concat({tap:read(buffer), tap.available, string.format('%.2f', buffer[1])}, ' ')"), "1000 300 0.50");
    EXPECT_EQ(fixture->lua("return #tap:read()"), "2400");
}

TEST_F(AudioTapLuaTest, CountsTheFramesThatAReaderLeftBehind) {
    const auto fixture = makeEngine();
    fixture->runLua("tap = audio.tapMaster({seconds = 0.01})");
    fixture->frames(2);
    EXPECT_EQ(fixture->lua("return tap.available .. ' ' .. tap.dropped"), "480 1120");
}

TEST_F(AudioTapLuaTest, ClosesWithItsOwnerAndOnRequest) {
    const auto fixture = makeEngine();
    fixture->runLua("owner = {} owned = audio.tapMaster({owner = owner}) mine = audio.tapMaster()");
    fixture->frames(1);
    fixture->runLua("owner = nil collectgarbage() collectgarbage()");
    fixture->frames(2);
    EXPECT_EQ(fixture->lua("return tostring(owned.closed) .. ' ' .. tostring(mine.closed)"), "true false");

    fixture->runLua("mine:close() before = mine.available");
    fixture->frames(1);
    EXPECT_EQ(fixture->lua("return tostring(mine.closed) .. ' ' .. tostring(mine.available == before)"), "true true");
}

TEST_F(AudioTapLuaTest, RefusesOptionsItCannotHold) {
    const auto fixture = makeEngine();
    EXPECT_NE(fixture->lua("audio.tapMaster({seconds = 0})").find("A tap of the audio holds more than 0 and at most 60 seconds of mix between two reads."), std::string::npos);
    EXPECT_NE(fixture->lua("audio.tapMaster({seconds = 61})").find("A tap of the audio holds more than 0 and at most 60 seconds of mix between two reads."), std::string::npos);
    EXPECT_NE(fixture->lua("audio.tapMaster({format = 'int16'})").find("Unknown option \"format\"."), std::string::npos);
    EXPECT_NE(fixture->lua("audio.tapMaster({owner = 3})").find("An owner must be a table or a userdata"), std::string::npos);
}

} // namespace haylen::lua::audio
