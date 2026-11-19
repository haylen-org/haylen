#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "audio/MixerFixture.hpp"
#include "haylen/audio/Filter.hpp"
#include "haylen/audio/Mixer.hpp"
#include "haylen/core/Connection.hpp"

namespace haylen::audio {

class MixerTest : public MixerFixture {};

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

    // The output mute silences everything after the buses and leaves their mutes alone.
    mixer.setOutputMuted(true);
    EXPECT_TRUE(mixer.isOutputMuted());
    EXPECT_FALSE(mixer.isBusMuted("master"));
    EXPECT_EQ(settledPeak(256), 0.0F);
    mixer.setOutputMuted(false);
    EXPECT_NEAR(settledPeak(256), 0.25F, 0.01F);

    EXPECT_EQ(mixer.getSampleRate(), kRate);
    EXPECT_EQ(mixer.getChannels(), 2U);
    EXPECT_FALSE(mixer.hasDevice());
    EXPECT_EQ(mixer.getBuses(), (std::vector<std::string>{"ambience", "master", "music", "sfx", "ui"}));
}

TEST_F(MixerTest, DeliversEveryDeviceEventWhenAListenerFails) {
    std::vector<Mixer::DeviceEvent> heard;
    // clang-format off
    const core::Connection connection = mixer.deviceEventReceived.connect([&heard](Mixer::DeviceEvent event) {
        heard.push_back(event);

        if (event == Mixer::DeviceEvent::InterruptionBegan) {
            throw std::runtime_error("The listener failed.");
        }
    });
    // clang-format on
    mixer.reportDeviceEvent(Mixer::DeviceEvent::InterruptionBegan);
    mixer.reportDeviceEvent(Mixer::DeviceEvent::RouteChanged);
    EXPECT_THROW(mixer.update(0.0F), std::runtime_error);
    EXPECT_EQ(heard, (std::vector{Mixer::DeviceEvent::InterruptionBegan, Mixer::DeviceEvent::RouteChanged}));
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

TEST_F(MixerTest, KeepsTheMusicWhenTheLimitOfTheNextTrackRefusesIt) {
    const Sound theme = makeTone(toFrames(1.0F));
    const Sound calm = makeTone(toFrames(1.0F), 0.25F);
    mixer.setLimit(theme, {.cooldown = 10.0F});
    mixer.playMusic(theme, {.fade = 0.0F});
    mixer.stopMusic(0.0F);
    EXPECT_FALSE(mixer.isActive(mixer.playMusic(theme, {.fade = 0.0F})));
    EXPECT_FALSE(mixer.getMusic().isValid());

    const Mixer::VoiceId playing = mixer.playMusic(calm, {.fade = 0.0F});
    EXPECT_FALSE(mixer.isActive(mixer.playMusic(theme, {.fade = 0.0F})));
    EXPECT_TRUE(mixer.isActive(playing));
    EXPECT_EQ(mixer.getMusic(), calm);
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

TEST_F(MixerTest, ReadsTheCursorOfAVoiceWhileTheMixingThreadMovesIt) {
    const Mixer::VoiceId voice = mixer.play(makeTone(toFrames(0.25F)), {.loop = true});
    std::atomic<bool> done{false};
    // clang-format off
    std::thread mixing([this, &done] {
        for (int block = 0; block < 400; ++block) {
            peak(256);
        }

        done = true;
    });
    // clang-format on
    float latest = 0.0F;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!done && std::chrono::steady_clock::now() < deadline) {
        latest = std::max(latest, mixer.getCursor(voice));
    }

    mixing.join();
    EXPECT_TRUE(done) << "The mixing thread did not finish its blocks in time.";
    EXPECT_LE(latest, 0.25F);
    EXPECT_GT(mixer.getCursor(voice), 0.0F);
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

    EXPECT_TRUE(mixer.getMusic().isValid());
    EXPECT_FALSE(mixer.isActive(voices[0]));
    EXPECT_TRUE(mixer.isActive(voices[7]));

    // The stolen voice fades out over the declick ramp before it leaves.
    peak(toFrames(Mixer::kDeclickSeconds) + 1024);
    mixer.update(1.0F / 60.0F);
    EXPECT_EQ(mixer.getVoiceCount(), 8U);
    mixer.stopAll();
    peak(toFrames(Mixer::kDeclickSeconds) + 1024);
    mixer.update(1.0F / 60.0F);
    EXPECT_EQ(mixer.getVoiceCount(), 0U);
}

TEST_F(MixerTest, LimitsTheVoicesOfOneSound) {
    const Sound coin = makeTone(toFrames(1.0F));
    const Sound other = makeTone(toFrames(1.0F));
    mixer.setLimit(coin, {.voices = 3});
    const Mixer::VoiceId first = mixer.play(coin, {.volume = 0.2F});
    const Mixer::VoiceId second = mixer.play(coin, {.volume = 0.9F});
    const Mixer::VoiceId third = mixer.play(coin, {.volume = 0.1F});
    const Mixer::VoiceId kept = mixer.play(other);
    const Mixer::VoiceId fourth = mixer.play(coin);
    EXPECT_FALSE(mixer.isActive(first));
    EXPECT_TRUE(mixer.isActive(second) && mixer.isActive(third) && mixer.isActive(fourth) && mixer.isActive(kept));

    // The quietest voice of the sound goes first, and a limit that steals nothing starts nothing.
    mixer.setLimit(coin, {.voices = 3, .steal = Mixer::Limit::Steal::Quietest});
    const Mixer::VoiceId fifth = mixer.play(coin);
    EXPECT_FALSE(mixer.isActive(third));
    EXPECT_TRUE(mixer.isActive(second));
    mixer.setLimit(coin, {.voices = 3, .steal = Mixer::Limit::Steal::None});
    EXPECT_FALSE(mixer.isActive(mixer.play(coin)));
    EXPECT_TRUE(mixer.isActive(fifth));
    const std::optional<Mixer::Limit> limit = mixer.getLimit(coin);
    ASSERT_TRUE(limit);
    EXPECT_EQ(limit->steal, Mixer::Limit::Steal::None);

    // A play within the cooldown starts nothing, and one after it plays.
    mixer.stopAll();
    mixer.setLimit(coin, {.cooldown = 0.03F});
    mixer.update(0.04F);
    EXPECT_TRUE(mixer.isActive(mixer.play(coin)));
    EXPECT_FALSE(mixer.isActive(mixer.play(coin)));
    mixer.update(0.04F);
    EXPECT_TRUE(mixer.isActive(mixer.play(coin)));
    mixer.setLimit(coin, {});
    EXPECT_FALSE(mixer.getLimit(coin).has_value());
    EXPECT_THROW(mixer.setLimit(Sound{}, {.voices = 1}), std::invalid_argument);
    EXPECT_THROW(mixer.setLimit(coin, {.cooldown = -1.0F}), std::invalid_argument);
}

TEST_F(MixerTest, StealsTheLowestPriorityFirst) {
    const Sound sound = makeTone(toFrames(1.0F));
    std::vector<Mixer::VoiceId> voices;
    for (int index = 0; index < 8; ++index) {
        voices.push_back(mixer.play(sound, {.priority = index == 5 ? std::uint8_t{10} : std::uint8_t{200}}));
    }

    const Mixer::VoiceId important = mixer.play(sound, {.priority = 250});
    EXPECT_FALSE(mixer.isActive(voices[5]));
    EXPECT_TRUE(mixer.isActive(voices[0]));
    EXPECT_TRUE(mixer.isActive(important));
}

TEST_F(MixerTest, StopsAndPausesWithoutDiscontinuities) {
    const Mixer::VoiceId voice = mixer.play(makeSine(440.0F), {.loop = true});
    peak(toFrames(0.1F));
    // clang-format off
    const auto largestStep = [this](std::size_t frames) {
        const std::vector<float> samples = renderLeft(frames);
        float largest = 0.0F;
        for (std::size_t index = 1; index < samples.size(); ++index) {
            largest = std::max(largest, std::abs(samples[index] - samples[index - 1]));
        }

        return largest;
    };
    // clang-format on

    // A sine of 440 hertz at 0.5 moves less than 0.03 between two samples, and a cut far from zero would jump by up to 0.5.
    mixer.setPaused(voice, true);
    EXPECT_LT(largestStep(2048), 0.04F);
    mixer.setPaused(voice, false);
    EXPECT_LT(largestStep(2048), 0.04F);
    mixer.stop(voice);
    EXPECT_LT(largestStep(2048), 0.04F);
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
    EXPECT_THROW(Mixer({.device = false, .sampleRate = 0}, jobs), std::invalid_argument);
    EXPECT_THROW(Mixer({.device = false, .maxVoices = 0}, jobs), std::invalid_argument);

    mixer.suspend();
    mixer.resume();
}

} // namespace haylen::audio
