#include <gtest/gtest.h>

#include <lua.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "audio/MixerFixture.hpp"
#include "audio/SoundData.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/audio/Filter.hpp"
#include "haylen/audio/Mixer.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Json.hpp"
#include "support/EngineFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::audio {

class SoundTest : public MixerFixture {};

class MixerTest : public MixerFixture {};

class AudioAssetTest : public MixerFixture {};

class AudioLuaTest : public MixerFixture {};

TEST_F(SoundTest, DecodesAndStreamsAudioFiles) {
    const std::vector<std::uint8_t> file = makeWav(22050, 2, makeSignal(2205, 2, 0.25F));

    const Sound decoded = Sound::decode(file);
    EXPECT_TRUE(decoded.isValid());
    EXPECT_FALSE(decoded.isStreamed());
    EXPECT_EQ(decoded.getChannels(), 2U);
    EXPECT_EQ(decoded.getSampleRate(), 22050U);
    EXPECT_EQ(decoded.getFrameCount(), 2205U);
    EXPECT_NEAR(decoded.getDuration(), 0.1F, 0.0001F);

    const Sound streamed = Sound::stream(file);
    EXPECT_TRUE(streamed.isStreamed());
    EXPECT_EQ(streamed.getFrameCount(), 2205U);
    EXPECT_NE(streamed, decoded);

    const Sound empty;
    EXPECT_FALSE(empty.isValid());
    EXPECT_EQ(empty.getDuration(), 0.0F);
    EXPECT_EQ(empty.getChannels(), 0U);
    EXPECT_THROW((void)Sound::decode(test::TestFiles::bytes("not audio")), std::runtime_error);
    EXPECT_THROW((void)Sound::stream({}), std::runtime_error);
    EXPECT_THROW((void)Sound::decode(makeWav(kRate, 1, {})), std::runtime_error);
}

TEST_F(SoundTest, MakesSoundsFromRawSamples) {
    const Sound floats = Sound::fromSamples(std::vector<float>{0.5F, -0.5F, 0.25F, -0.25F}, 2, 24000);
    EXPECT_FALSE(floats.isStreamed());
    EXPECT_EQ(floats.getChannels(), 2U);
    EXPECT_EQ(floats.getSampleRate(), 24000U);
    EXPECT_EQ(floats.getFrameCount(), 2U);
    EXPECT_EQ(floats.getData()->samples, (std::vector<float>{0.5F, -0.5F, 0.25F, -0.25F}));

    const Sound shorts = Sound::fromSamples(std::vector<std::int16_t>{16384, -32768, 0}, 1, 8000);
    EXPECT_EQ(shorts.getData()->samples, (std::vector<float>{0.5F, -1.0F, 0.0F}));
    EXPECT_NEAR(shorts.getDuration(), 3.0F / 8000.0F, 1e-7F);

    EXPECT_THROW((void)Sound::fromSamples(std::vector<float>{0.5F, 0.5F, 0.5F}, 2, 8000), std::invalid_argument);
    EXPECT_THROW((void)Sound::fromSamples(std::vector<float>{0.5F}, 1, 0), std::invalid_argument);
    EXPECT_THROW((void)Sound::fromSamples(std::vector<float>{}, 1, 8000), std::runtime_error);
}

TEST_F(MixerTest, MixesVoicesThroughBuses) {
    const Sound sound = makeTone(toFrames(1.0F));
    const Mixer::VoiceId voice = mixer.play(sound);
    EXPECT_TRUE(mixer.isActive(voice));
    EXPECT_NEAR(peak(256), 0.5F, 0.01F);

    mixer.setBusVolume("sfx", 0.0F);
    EXPECT_EQ(settledPeak(256), 0.0F);
    EXPECT_EQ(mixer.getBusVolume("sfx"), 0.0F);
    mixer.setBusVolume("sfx", 1.0F);
    mixer.setBusMuted("master", true);
    EXPECT_TRUE(mixer.isBusMuted("master"));
    EXPECT_EQ(settledPeak(256), 0.0F);
    mixer.setBusMuted("master", false);
    mixer.setVolume(voice, 0.5F);
    EXPECT_NEAR(settledPeak(256), 0.25F, 0.01F);

    EXPECT_EQ(mixer.getSampleRate(), kRate);
    EXPECT_EQ(mixer.getChannels(), 2U);
    EXPECT_FALSE(mixer.hasDevice());
    EXPECT_EQ(mixer.getBuses(), (std::vector<std::string>{"ambience", "master", "music", "sfx", "ui"}));
}

TEST_F(MixerTest, ReleasesVoicesWhenTheyFinish) {
    const Mixer::VoiceId voice = mixer.play(makeTone(toFrames(0.05F)));
    EXPECT_EQ(mixer.getVoiceCount(), 1U);
    EXPECT_GT(mixer.getCursor(voice), -1.0F);

    peak(toFrames(0.1F));
    mixer.update(1.0F / 60.0F);
    EXPECT_EQ(mixer.getVoiceCount(), 0U);
    EXPECT_FALSE(mixer.isActive(voice));
    EXPECT_EQ(mixer.getCursor(voice), 0.0F);

    const Mixer::VoiceId looping = mixer.play(makeTone(toFrames(0.05F)), {.loop = true});
    peak(toFrames(0.2F));
    mixer.update(1.0F / 60.0F);
    EXPECT_TRUE(mixer.isActive(looping));
}

TEST_F(MixerTest, ControlsPlayback) {
    const Sound sound = makeTone(toFrames(0.1F));

    const Mixer::VoiceId fast = mixer.play(sound, {.pitch = 2.0F});
    const Mixer::VoiceId late = mixer.play(sound, {.startAt = 0.09F});
    const Mixer::VoiceId normal = mixer.play(makeTone(toFrames(1.0F)));
    peak(toFrames(0.08F));
    mixer.update(1.0F / 60.0F);
    EXPECT_FALSE(mixer.isActive(fast));
    EXPECT_FALSE(mixer.isActive(late));
    EXPECT_TRUE(mixer.isActive(normal));

    mixer.setPaused(normal, true);
    EXPECT_EQ(settledPeak(256), 0.0F);
    mixer.update(1.0F / 60.0F);
    EXPECT_TRUE(mixer.isActive(normal));
    mixer.setPaused(normal, false);
    EXPECT_GT(settledPeak(256), 0.0F);
    mixer.stop(normal);
    EXPECT_EQ(settledPeak(256), 0.0F);

    const Mixer::VoiceId fading = mixer.play(sound, {.loop = true, .fadeIn = 0.5F});
    EXPECT_LT(peak(480), 0.05F);
    EXPECT_NEAR(peak(toFrames(0.6F)), 0.5F, 0.01F);
    mixer.stop(fading, 0.05F);
    EXPECT_FALSE(mixer.isActive(fading)) << "A stopped voice is over while it fades out.";
    EXPECT_GT(peak(480), 0.0F);
    peak(toFrames(0.1F));
    mixer.update(1.0F / 60.0F);
    EXPECT_EQ(mixer.getVoiceCount(), 0U);

    mixer.setPitch(fading, 3.0F);
    mixer.setPan(fading, 1.0F);
    mixer.setVolume(fading, 1.0F);
    mixer.setPosition(fading, {});
    mixer.setPaused(fading, true);
    mixer.stop(fading);
    EXPECT_FALSE(mixer.isActive(fading));
}

TEST_F(MixerTest, VariesPitchWithinTheRange) {
    const Sound sound = makeTone(toFrames(1.0F));
    mixer.seedVariation(42);
    std::vector<float> pitches;
    for (int play = 0; play < 6; ++play) {
        const Mixer::VoiceId voice = mixer.play(sound, {.pitch = 1.2F, .pitchVariation = 0.1F});
        pitches.push_back(mixer.getPitch(voice));
        mixer.stop(voice);
    }
    for (const float pitch : pitches) {
        EXPECT_GE(pitch, 1.1F);
        EXPECT_LE(pitch, 1.3F);
    }
    EXPECT_NE(pitches.front(), pitches.back());

    mixer.seedVariation(42);
    const Mixer::VoiceId again = mixer.play(sound, {.pitch = 1.2F, .pitchVariation = 0.1F});
    EXPECT_EQ(mixer.getPitch(again), pitches.front());
    EXPECT_EQ(mixer.getPitch(mixer.play(sound, {.pitch = 0.8F})), 0.8F);
    EXPECT_EQ(mixer.getPitch(9999), 0.0F);
    EXPECT_THROW((void)mixer.play(sound, {.pitch = 1.0F, .pitchVariation = 1.0F}), std::invalid_argument);
    EXPECT_THROW((void)mixer.play(sound, {.pitchVariation = -0.1F}), std::invalid_argument);
}

TEST_F(MixerTest, RejectsValuesThatAreNotFinite) {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    const Sound sound = makeTone(toFrames(1.0F));
    const Mixer::VoiceId voice = mixer.play(sound, {.loop = true});

    EXPECT_THROW((void)mixer.play(sound, {.volume = nan}), std::invalid_argument);
    EXPECT_THROW((void)mixer.play(sound, {.pitch = 0.0F}), std::invalid_argument);
    EXPECT_THROW((void)mixer.play(sound, {.pitch = infinity}), std::invalid_argument);
    EXPECT_THROW((void)mixer.play(sound, {.pitchVariation = nan}), std::invalid_argument);
    EXPECT_THROW((void)mixer.play(sound, {.pan = infinity}), std::invalid_argument);
    EXPECT_THROW((void)mixer.play(sound, {.fadeIn = infinity}), std::invalid_argument);
    EXPECT_THROW((void)mixer.play(sound, {.startAt = nan}), std::invalid_argument);
    EXPECT_THROW((void)mixer.play(sound, {.position = math::Vec2{nan, 0.0F}}), std::invalid_argument);
    EXPECT_THROW(mixer.setVolume(voice, infinity), std::invalid_argument);
    EXPECT_THROW(mixer.setPitch(voice, nan), std::invalid_argument);
    EXPECT_THROW(mixer.setPitch(voice, -1.0F), std::invalid_argument);
    EXPECT_THROW(mixer.setPan(voice, nan), std::invalid_argument);
    EXPECT_THROW(mixer.setPosition(voice, {0.0F, infinity}), std::invalid_argument);
    EXPECT_THROW(mixer.setListener({nan, 0.0F}), std::invalid_argument);
    EXPECT_THROW(mixer.setBusVolume("sfx", infinity), std::invalid_argument);
    EXPECT_THROW(mixer.setSpatialization({.doppler = 1.0F, .speedOfSound = infinity}), std::invalid_argument);

    // Nothing reached the mix, so the voice still plays as it did.
    EXPECT_EQ(mixer.getVoiceCount(), 1U);
    EXPECT_NEAR(settledPeak(256), 0.5F, 0.01F);
}

TEST_F(MixerTest, PansAndAttenuatesPositionalVoices) {
    mixer.setSpatialization({.minDistance = 100.0F, .maxDistance = 200.0F, .panDistance = 200.0F});
    mixer.setListener({10.0F, 10.0F});
    EXPECT_EQ(mixer.getListener(), math::Vec2(10.0F, 10.0F));

    const Mixer::VoiceId voice = mixer.play(makeTone(toFrames(1.0F)), {.loop = true, .position = math::Vec2{400.0F, 10.0F}});
    EXPECT_EQ(settledPeak(256), 0.0F);

    mixer.setListener({330.0F, 10.0F});
    mixer.update(1.0F / 60.0F);
    EXPECT_GT(settledPeak(256), 0.4F);

    mixer.setPosition(voice, {-1000.0F, 10.0F});
    mixer.setListener({-1150.0F, 10.0F});
    mixer.update(1.0F / 60.0F);
    const float mixed = settledPeak(256);
    EXPECT_GT(mixed, 0.0F);
    EXPECT_LT(mixed, 0.5F);

    EXPECT_THROW(mixer.setSpatialization({.minDistance = 50.0F, .maxDistance = 50.0F}), std::invalid_argument);
    EXPECT_THROW(mixer.setSpatialization({.minDistance = -1.0F, .maxDistance = 50.0F}), std::invalid_argument);
}

TEST_F(MixerTest, CrossfadesMusic) {
    const Sound first = makeTone(toFrames(1.0F));
    const Sound second = makeTone(toFrames(1.0F), 0.25F);

    mixer.playMusic(first, {.fade = 0.05F});
    EXPECT_EQ(mixer.getMusic(), first);
    mixer.playMusic(first, {.volume = 0.5F});
    EXPECT_EQ(mixer.getVoiceCount(), 1U);

    mixer.playMusic(second, {.fade = 0.05F});
    EXPECT_EQ(mixer.getVoiceCount(), 2U);
    peak(toFrames(0.1F));
    mixer.update(1.0F / 60.0F);
    EXPECT_EQ(mixer.getVoiceCount(), 1U);
    EXPECT_EQ(mixer.getMusic(), second);
    EXPECT_NEAR(settledPeak(256), 0.25F, 0.01F);

    mixer.stopMusic(0.0F);
    mixer.update(1.0F / 60.0F);
    EXPECT_FALSE(mixer.getMusic().isValid());
    mixer.stopMusic();
}

TEST_F(MixerTest, ControlsMusicThroughItsVoice) {
    const Sound first = makeTone(toFrames(1.0F));
    const Sound second = makeTone(toFrames(1.0F), 0.25F);

    // The voice of the track pauses the music alone, and asking for the same track keeps it paused.
    const Mixer::VoiceId music = mixer.playMusic(first, {.fade = 0.0F});
    const Mixer::VoiceId effect = mixer.play(second, {.loop = true});
    mixer.setPaused(music, true);
    EXPECT_NEAR(settledPeak(256), 0.25F, 0.01F);
    EXPECT_EQ(mixer.playMusic(first, {.volume = 0.5F}), music);
    EXPECT_TRUE(mixer.isPaused(music));
    EXPECT_EQ(mixer.getMusic(), first);
    mixer.setPaused(music, false);
    EXPECT_NEAR(settledPeak(256), 0.5F, 0.01F);
    mixer.stop(effect);

    // The next track replaces a paused one and plays.
    mixer.setPaused(music, true);
    const Mixer::VoiceId next = mixer.playMusic(second, {.fade = 0.0F});
    EXPECT_NE(next, music);
    EXPECT_NEAR(settledPeak(256), 0.25F, 0.01F);
    mixer.update(1.0F / 60.0F);
    EXPECT_FALSE(mixer.isActive(music));

    // Stopping the voice ends the music while it fades out, so asking for the track again starts it over.
    mixer.stop(next, 0.5F);
    EXPECT_FALSE(mixer.getMusic().isValid());
    EXPECT_NE(mixer.playMusic(second), next);
    EXPECT_EQ(mixer.getMusic(), second);
}

// A player moves through a track with the cursor of its voice, which stays within the sound, and a music track starts where the app asks.
TEST_F(MixerTest, MovesTheCursorOfAVoice) {
    const Mixer::VoiceId voice = mixer.play(makeTone(toFrames(1.0F)));
    mixer.setCursor(voice, 0.5F);
    peak(toFrames(0.1F));
    EXPECT_NEAR(mixer.getCursor(voice), 0.6F, 0.02F);
    mixer.setCursor(voice, -2.0F);
    peak(toFrames(0.1F));
    EXPECT_NEAR(mixer.getCursor(voice), 0.1F, 0.02F);
    mixer.setCursor(voice, 5.0F);
    peak(toFrames(0.1F));
    mixer.update(1.0F / 60.0F);
    EXPECT_FALSE(mixer.isActive(voice)) << "A cursor past the end ends the sound.";
    mixer.setCursor(voice, 0.0F);
    EXPECT_THROW(mixer.setCursor(voice, std::numeric_limits<float>::quiet_NaN()), std::invalid_argument);

    const Mixer::VoiceId music = mixer.playMusic(makeTone(toFrames(2.0F)), {.fade = 0.0F, .startAt = 1.5F});
    peak(toFrames(0.1F));
    EXPECT_NEAR(mixer.getCursor(music), 1.6F, 0.02F);
    mixer.setPaused(music, true);
    mixer.setCursor(music, 0.5F);
    peak(toFrames(0.1F));
    EXPECT_NEAR(mixer.getCursor(music), 0.5F, 0.02F);
    mixer.setPaused(music, false);
    peak(toFrames(0.1F));
    EXPECT_NEAR(mixer.getCursor(music), 0.6F, 0.02F);
}

TEST_F(MixerTest, FailedPlaysLeaveEveryVoicePlaying) {
    const Sound track = makeTone(toFrames(1.0F));
    const Mixer::VoiceId music = mixer.playMusic(track, {.fade = 0.0F});
    EXPECT_THROW(mixer.playMusic(Sound{}), std::invalid_argument);
    EXPECT_THROW(mixer.playMusic(makeTone(10), {.bus = "missing"}), std::invalid_argument);
    EXPECT_EQ(mixer.getMusic(), track);
    EXPECT_TRUE(mixer.isActive(music));

    // A full mixer makes room only for a voice that plays.
    std::vector<Mixer::VoiceId> voices;
    for (int index = 0; index < 7; ++index) {
        voices.push_back(mixer.play(track));
    }
    const auto busy = std::make_shared<Filter>(Filter::Kind::Lowpass, Filter::Settings{});
    mixer.addBusEffect("ui", busy);
    EXPECT_THROW((void)mixer.play(track, {.effects = {busy}}), std::invalid_argument);
    EXPECT_EQ(mixer.getVoiceCount(), 8U);
    EXPECT_TRUE(mixer.isActive(voices.front()));
}

TEST_F(MixerTest, StealsTheOldestVoiceWhenFull) {
    const Sound sound = makeTone(toFrames(1.0F));
    mixer.playMusic(sound);
    std::vector<Mixer::VoiceId> voices;
    for (int index = 0; index < 8; ++index) {
        voices.push_back(mixer.play(sound));
    }
    EXPECT_EQ(mixer.getVoiceCount(), 8U);
    EXPECT_TRUE(mixer.getMusic().isValid());
    EXPECT_FALSE(mixer.isActive(voices[0]));
    EXPECT_TRUE(mixer.isActive(voices[7]));

    mixer.stopAll();
    mixer.update(1.0F / 60.0F);
    EXPECT_EQ(mixer.getVoiceCount(), 0U);
}

TEST_F(MixerTest, ManagesBusesAndRejectsInvalidUse) {
    mixer.createBus("footsteps", "sfx");
    const std::vector<std::string> buses = mixer.getBuses();
    EXPECT_NE(std::ranges::find(buses, "footsteps"), buses.end());
    mixer.play(makeTone(toFrames(1.0F)), {.bus = "footsteps"});
    mixer.setBusVolume("sfx", 0.5F, 0.0F);
    EXPECT_NEAR(settledPeak(256), 0.25F, 0.01F);

    EXPECT_THROW(mixer.createBus("footsteps"), std::invalid_argument);
    EXPECT_THROW(mixer.createBus(""), std::invalid_argument);
    EXPECT_THROW(mixer.createBus("steps", "missing"), std::invalid_argument);
    EXPECT_THROW(mixer.play(makeTone(10), {.bus = "missing"}), std::invalid_argument);
    EXPECT_THROW(mixer.play(Sound{}), std::invalid_argument);
    EXPECT_THROW((void)mixer.getBusVolume("missing"), std::invalid_argument);
    EXPECT_THROW(Mixer({.device = false, .sampleRate = 0}), std::invalid_argument);
    EXPECT_THROW(Mixer({.device = false, .maxVoices = 0}), std::invalid_argument);

    mixer.suspend();
    mixer.resume();
}

TEST_F(MixerTest, StreamsLongSounds) {
    const Sound music = Sound::stream(makeWav(kRate, 2, makeSignal(toFrames(0.2F), 2, 0.5F)));
    const Mixer::VoiceId voice = mixer.play(music, {.bus = "music"});
    EXPECT_NEAR(peak(toFrames(0.1F)), 0.5F, 0.01F);
    peak(toFrames(0.2F));
    mixer.update(1.0F / 60.0F);
    EXPECT_FALSE(mixer.isActive(voice));
}

TEST_F(SoundTest, ReadsTheSessionFromAppJson) {
    const core::AppConfig config = core::AppConfig::fromJson(core::Json::parse(R"({"audio": {"iosSession": "playback", "mixWithOthers": true}})"));
    EXPECT_EQ(config.audioSession.category, Session::Category::Playback);
    EXPECT_TRUE(config.audioSession.mixWithOthers);
    EXPECT_EQ(config.toJson().at("audio"), core::Json::parse(R"({"iosSession": "playback", "mixWithOthers": true})"));
    EXPECT_EQ(core::AppConfig::fromJson(core::Json::parse(R"({"audio": {"iosSession": "soloAmbient"}})")).toJson().at("audio").at("iosSession"), "soloAmbient");
    EXPECT_EQ(core::AppConfig{}.audioSession.category, Session::Category::Ambient);

    EXPECT_THROW((void)core::AppConfig::fromJson(core::Json::parse(R"({"audio": {"iosSession": "record"}})")), std::invalid_argument);
    EXPECT_THROW((void)core::AppConfig::fromJson(core::Json::parse(R"({"audio": {"focus": true}})")), std::invalid_argument);
    EXPECT_THROW((void)core::AppConfig::fromJson(core::Json::parse(R"({"audio": {"mixWithOthers": true}})")), std::invalid_argument);
    EXPECT_THROW(Mixer({.device = false, .session = {.category = Session::Category::Ambient, .mixWithOthers = true}}), std::invalid_argument);
    EXPECT_NO_THROW(Mixer({.device = false, .session = {.category = Session::Category::Playback, .mixWithOthers = true}}));
}

TEST_F(AudioAssetTest, LoadsSoundsThroughTheAssetManager) {
    const std::vector<std::uint8_t> wav = makeWav(kRate, 1, makeSignal(480, 1, 0.5F));
    test::EngineFixture fixture({{"content/sfx/hit.wav", std::string(wav.begin(), wav.end())}});
    assets::Manager& assets = fixture.engine().getAssets();

    EXPECT_EQ(assets.getTypeForPath("sfx/hit.wav"), "sound");
    const auto decoded = assets.load("sound", "sfx/hit.wav");
    EXPECT_EQ(assets.load("sound", "sfx/hit.wav", {{"stream", false}}), decoded);
    EXPECT_NE(assets.load("sound", "sfx/hit.wav", {{"stream", true}}), decoded);
    EXPECT_THROW((void)assets.load("sound", "sfx/hit.wav", {{"loop", true}}), std::invalid_argument);
    EXPECT_EQ(fixture.engine().getAudio().getSampleRate(), 48000U);
}

TEST_F(AudioLuaTest, PlaysSoundsFromLua) {
    const std::vector<std::uint8_t> hit = makeWav(kRate, 1, makeSignal(4800, 1, 0.5F));
    const std::vector<std::uint8_t> theme = makeWav(kRate, 2, makeSignal(48000, 2, 0.25F));
    test::EngineFixture fixture({{"content/sfx/hit.wav", std::string(hit.begin(), hit.end())}, {"content/music/theme.wav", std::string(theme.begin(), theme.end())}});
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
    test::EngineFixture fixture;
    const std::vector<std::uint8_t> wav = makeWav(22050, 2, makeSignal(2205, 2, 0.25F));
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

} // namespace haylen::audio
