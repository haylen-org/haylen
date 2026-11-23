#include <gtest/gtest.h>

#include <lua.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include "audio/MixerFixture.hpp"
#include "support/LuaFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::lua::audio {

class AudioLuaTest : public haylen::audio::MixerFixture {};

TEST_F(AudioLuaTest, PlaysSoundsFromLua) {
    const std::vector<std::uint8_t> hit = test::TestFiles::wav(kRate, 1, makeSignal(4800, 1, 0.5F));
    const std::vector<std::uint8_t> theme = test::TestFiles::wav(kRate, 2, makeSignal(48000, 2, 0.25F));
    test::LuaFixture fixture({{"content/sfx/hit.wav", std::string(hit.begin(), hit.end())}, {"content/music/theme.wav", std::string(theme.begin(), theme.end())}});
    fixture.runLua("audio = require('haylen.audio') assets = require('haylen.assets') hit = assets.load('sfx/hit.wav') theme = assets.load('music/theme.wav', nil, {stream = true})");

    EXPECT_EQ(fixture.lua("return hit.channels .. ' ' .. hit.sampleRate .. ' ' .. hit.frameCount .. ' ' .. tostring(hit.streamed) .. ' ' .. tostring(math.abs(hit.duration - 0.1) < 1e-6)"), "1 48000 4800 false true");
    EXPECT_EQ(fixture.lua("return theme.streamed and theme == assets.load('music/theme.wav', 'sound', {stream = true}) and theme ~= hit"), "true");
    EXPECT_EQ(fixture.lua("return audio.sampleRate() .. ' ' .. audio.channels() .. ' ' .. tostring(audio.hasDevice()) .. ' ' .. tostring(audio.outputAvailable())"), "48000 2 false false");

    // clang-format off
    const std::string varied = fixture.lua(R"(
        local function pitches()
            audio.seedVariation(7)
            local list = {}
            for index = 1, 3 do
                local voice = audio.play(hit, {pitch = 1, pitchVariation = 0.3})
                list[index] = audio.pitch(voice)
                audio.stop(voice)
            end
            return table.concat(list, ',')
        end
        local first = pitches()
        return tostring(first == pitches()) .. ' ' .. tostring(first ~= '1.0,1.0,1.0')
    )");
    // clang-format on
    EXPECT_EQ(varied, "true true");
    EXPECT_NE(fixture.lua("audio.seedVariation(-1)").find("expected a non-negative integer"), std::string::npos);
    fixture.frames(1);

    fixture.runLua("voice = audio.play(hit, {bus = 'ui', volume = 0.5, pitch = 1.5, pan = -0.5, loop = true, fadeIn = 0.1, startAt = 0.01, x = 10, y = 20})");
    EXPECT_EQ(fixture.lua("return audio.active(voice) and audio.voiceCount() == 1 and audio.cursor(voice) >= 0"), "true");
    EXPECT_EQ(fixture.lua("local varied = audio.play(hit, {pitch = 1, pitchVariation = 0.2}) local p = audio.pitch(varied) audio.stop(varied) return tostring(p >= 0.8 and p <= 1.2) .. ' ' .. audio.pitch(voice)"), "true 1.5");
    fixture.runLua("audio.pause(voice) audio.resume(voice) audio.setVolume(voice, 1) audio.setPitch(voice, 1) audio.setPan(voice, 0) audio.setPosition(voice, 0, 0)");
    fixture.runLua("audio.stop(voice) second = audio.play(hit) audio.stopAll(0.1)");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return audio.active(voice)"), "false");

    fixture.runLua("music = audio.playMusic(theme, {volume = 0.8, fade = 0, loop = false, bus = 'music', startAt = 0.01}) audio.setCursor(music, 0.02) audio.pause(music)");
    EXPECT_EQ(fixture.lua("return tostring(audio.music() == theme) .. ' ' .. tostring(audio.paused(music)) .. ' ' .. tostring(audio.playMusic(theme) == music)"), "true true true");
    fixture.runLua("audio.stopMusic(0)");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return audio.music()"), "nil");

    fixture.runLua("audio.createBus('steps', 'sfx') audio.createBus('voices') audio.setBusVolume('steps', 0.3, 0.5) audio.setBusMuted('voices', true)");
    EXPECT_EQ(fixture.lua("return audio.busVolume('steps') > 0.29 and audio.busMuted('voices') and #audio.buses() == 7"), "true");
    fixture.runLua("audio.setListener(5, 6) audio.setSpatialization({minDistance = 10, maxDistance = 20})");
    EXPECT_EQ(fixture.lua("local x, y = audio.listener() return x .. ',' .. y"), "5.0,6.0");

    EXPECT_EQ(fixture.lua("audio.setLimit(hit, {voices = 2, cooldown = 0.5, steal = 'quietest'}) local limit = audio.limit(hit) return limit.voices .. ' ' .. limit.cooldown .. ' ' .. limit.steal"), "2 0.5 quietest");
    EXPECT_EQ(fixture.lua("local first = audio.play(hit, {priority = 255}) local second = audio.play(hit) return tostring(audio.active(first)) .. ' ' .. tostring(audio.active(second))"), "true false");
    EXPECT_EQ(fixture.lua("audio.setLimit(hit, {}) return audio.limit(hit)"), "nil");
    EXPECT_NE(fixture.lua("audio.setLimit(hit, {steal = 'newest'})").find("The \"steal\" of a sound limit must be \"oldest\", \"quietest\" or \"none\"."), std::string::npos);
    EXPECT_NE(fixture.lua("audio.setLimit(hit, {voice = 1})").find("Unknown option \"voice\"."), std::string::npos);
    EXPECT_NE(fixture.lua("audio.setLimit(hit, {cooldown = -1})").find("The cooldown of a sound must be a finite number of seconds from 0."), std::string::npos);
    EXPECT_NE(fixture.lua("audio.play(hit, {priority = 300})").find("The priority of a voice runs from 0 to 255."), std::string::npos);
    EXPECT_NE(fixture.lua("audio.play(hit, {volumen = 1})").find("Unknown option \"volumen\""), std::string::npos);
    EXPECT_NE(fixture.lua("audio.playMusic(theme, {crossfade = 1})").find("Unknown option \"crossfade\""), std::string::npos);
    EXPECT_NE(fixture.lua("audio.play('hit')").find("error: "), std::string::npos);
    EXPECT_NE(fixture.lua("audio.play(hit, {bus = 'missing'})").find("The audio bus \"missing\" does not exist."), std::string::npos);
    EXPECT_NE(fixture.lua("audio.play(hit, {pitch = 0})").find("Audio needs a finite pitch above 0."), std::string::npos);
    EXPECT_NE(fixture.lua("audio.setVolume(1, 0 / 0)").find("Audio needs a finite volume."), std::string::npos);
    EXPECT_NE(fixture.lua("audio.setCursor(1, 0 / 0)").find("Audio needs a finite cursor."), std::string::npos);
    EXPECT_NE(fixture.lua("return hit.loudness").find("Sound\" has no member \"loudness\""), std::string::npos);
}

// The bytes of a sound file or of raw samples, such as the audio a plugin returns, become a sound.
TEST_F(AudioLuaTest, MakesSoundsFromBytes) {
    test::LuaFixture fixture;
    const std::vector<std::uint8_t> wav = test::TestFiles::wav(22050, 2, makeSignal(2205, 2, 0.25F));
    lua_pushlstring(fixture.lua(), reinterpret_cast<const char*>(wav.data()), wav.size());
    lua_setglobal(fixture.lua(), "wav");
    fixture.runLua("audio = require('haylen.audio')");

    EXPECT_EQ(fixture.lua("local s = audio.newSound(wav) return s.channels .. ' ' .. s.sampleRate .. ' ' .. s.frameCount .. ' ' .. tostring(s.streamed)"), "2 22050 2205 false");
    EXPECT_EQ(fixture.lua("return tostring(audio.newSound(wav, {stream = true}).streamed)"), "true");
    EXPECT_EQ(fixture.lua("local s = audio.newSound(string.pack('<ffff', 0.5, -0.5, 0.25, 0), {format = 'float32', sampleRate = 16000, channels = 2}) return s.frameCount .. ' ' .. s.sampleRate"), "2 16000");
    EXPECT_EQ(fixture.lua("local s = audio.newSound(string.pack('<hhh', 1, 2, 3), {format = 'int16', sampleRate = 8000, channels = 1}) return s.frameCount .. ' ' .. tostring(audio.active(audio.play(s)))"), "3 true");

    EXPECT_NE(fixture.lua("audio.newSound('not audio')").find("Audio data is not a supported"), std::string::npos);
    EXPECT_NE(fixture.lua("audio.newSound('abc', {format = 'int16', sampleRate = 8000, channels = 1})").find("Samples of this format take 2 bytes each, which 3 bytes do not fill."), std::string::npos);
    EXPECT_NE(fixture.lua("audio.newSound('abcd', {format = 'int8', sampleRate = 8000, channels = 1})").find("The format of raw samples is \"float32\" or \"int16\", not \"int8\"."), std::string::npos);
    EXPECT_NE(fixture.lua("audio.newSound('abcd', {format = 'int16'})").find("Raw audio needs a sample rate and channels."), std::string::npos);
    EXPECT_NE(fixture.lua("audio.newSound(wav, {sampleRate = 8000})").find("Only raw samples take a sample rate and channels"), std::string::npos);
    EXPECT_NE(fixture.lua("audio.newSound('abcd', {format = 'int16', sampleRate = 8000, channels = 1, stream = true})").find("cannot stream"), std::string::npos);
    EXPECT_NE(fixture.lua("audio.newSound(wav, {loop = true})").find("Unknown option \"loop\""), std::string::npos);
}

} // namespace haylen::lua::audio
