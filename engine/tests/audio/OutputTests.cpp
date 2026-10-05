#include <gtest/gtest.h>
#include <miniaudio.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "audio/Device.hpp"
#include "audio/MixerFixture.hpp"
#include "audio/OutputBackend.hpp"
#include "haylen/core/Log.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::audio {

// Plays through a backend that refuses the audio until a test lets it through, the way a browser without `AudioWorklet` or an iOS audio session that another app holds refuses it, and that a test can keep waiting the way an audio service that hangs does. The device it opens never asks for samples.
class AudioOutputTest : public MixerFixture {
  protected:
    static constexpr float kFrameSeconds = 1.0F / 60.0F;

    void SetUp() override {
        accepting = false;
        answering = true;
        closedDevices = 0;
        // clang-format off
        listener = core::Log::addListener([this](core::Log::Level level, std::string_view line) {
            const std::scoped_lock lock(linesMutex);
            lines.emplace_back(level, std::string(line));
        });
        // clang-format on
    }

    void TearDown() override {
        answer();
        core::Log::removeListener(listener);
    }

    static ma_result initContext(ma_context*, const ma_context_config*, ma_backend_callbacks* callbacks) {
        {
            std::unique_lock lock(gateMutex);
            gate.wait(lock, [] { return answering; });
        }
        if (!accepting) {
            return MA_FAILED_TO_INIT_BACKEND;
        }
        callbacks->onDeviceInit = &initDevice;
        callbacks->onDeviceUninit = &uninitDevice;
        callbacks->onDeviceStart = &ignore;
        callbacks->onDeviceStop = &ignore;
        return MA_SUCCESS;
    }

    static ma_result initDevice(ma_device*, const ma_device_config*, ma_device_descriptor* playback, ma_device_descriptor*) {
        playback->format = ma_format_f32;
        ma_channel_map_init_standard(ma_standard_channel_map_default, playback->channelMap, MA_MAX_CHANNELS, playback->channels);
        playback->periodSizeInFrames = 256;
        playback->periodCount = 2;
        return MA_SUCCESS;
    }

    static ma_result uninitDevice(ma_device*) {
        ++closedDevices;
        return MA_SUCCESS;
    }

    static ma_result ignore(ma_device*) {
        return MA_SUCCESS;
    }

    // Keeps every opening waiting until `answer`, the way an audio service that hangs keeps the system from answering.
    static void hang() {
        const std::scoped_lock lock(gateMutex);
        answering = false;
    }

    static void answer() {
        {
            const std::scoped_lock lock(gateMutex);
            answering = true;
        }
        gate.notify_all();
    }

    // Updates the mixer frame by frame over the seconds, as the engine does.
    static void play(Mixer& target, float seconds) {
        for (float elapsed = 0.0F; elapsed + kFrameSeconds / 2.0F < seconds; elapsed += kFrameSeconds) {
            target.update(kFrameSeconds);
        }
    }

    // Waits for the thread that opens the device, updating the mixer without letting time pass, as the frames of an app that the system keeps waiting do.
    static bool settle(Mixer& target) {
        // clang-format off
        return waitFor([&target] {
            target.update(0.0F);
            return !target.isOutputOpening();
        });
        // clang-format on
    }

    static bool waitFor(const std::function<bool()>& condition) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!condition()) {
            if (std::chrono::steady_clock::now() > deadline) {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return true;
    }

    [[nodiscard]] std::vector<std::string> getLines(core::Log::Level level) {
        const std::scoped_lock lock(linesMutex);
        std::vector<std::string> found;
        for (const auto& [lineLevel, line] : lines) {
            if (lineLevel == level) {
                found.push_back(line);
            }
        }
        return found;
    }

    [[nodiscard]] std::vector<std::string> getWarnings() {
        return getLines(core::Log::Level::Warning);
    }

    static inline std::atomic<bool> accepting = false;
    static inline bool answering = true;
    static inline std::mutex gateMutex;
    static inline std::condition_variable gate;
    static inline std::atomic<int> closedDevices = 0;
    static constexpr OutputBackend kBackend{.initContext = &initContext, .refusal = "the test refuses the audio"};

    std::mutex linesMutex;
    std::vector<std::pair<core::Log::Level, std::string>> lines;
    std::uint64_t listener = 0;
};

TEST_F(AudioOutputTest, KeepsTimeWithoutSoundWhileTheSystemRefusesTheOutput) {
    Mixer refused({.device = true, .sampleRate = kRate, .channels = 2, .backend = &kBackend});
    EXPECT_TRUE(refused.hasDevice());
    ASSERT_TRUE(settle(refused));
    EXPECT_FALSE(refused.isOutputAvailable());
    ASSERT_EQ(getWarnings().size(), 1U);
    EXPECT_NE(getWarnings().front().find("the test refuses the audio"), std::string::npos);
    std::vector<float> samples(64);
    EXPECT_THROW(refused.render(samples), std::logic_error);

    // Voices move on in real time and end without anyone hearing them.
    const Mixer::VoiceId shot = refused.play(makeTone(toFrames(0.25F)));
    play(refused, 0.2F);
    EXPECT_TRUE(refused.isActive(shot));
    EXPECT_NEAR(refused.getCursor(shot), 0.2F, 0.01F);
    play(refused, 0.1F);
    EXPECT_FALSE(refused.isActive(shot));
    EXPECT_EQ(refused.getVoiceCount(), 0U);

    // Music crossfades, and the track it replaces goes once its fade is over.
    refused.playMusic(makeTone(toFrames(2.0F)), {.fade = 0.0F});
    const Mixer::VoiceId night = refused.playMusic(makeTone(toFrames(2.0F), 0.25F), {.fade = 0.5F});
    play(refused, 0.3F);
    EXPECT_EQ(refused.getVoiceCount(), 2U);
    play(refused, 0.3F);
    EXPECT_EQ(refused.getVoiceCount(), 1U);
    EXPECT_TRUE(refused.isActive(night));

    // No time passes while the app is in the background or an interruption holds the audio, and an interruption that ends while the system still refuses the audio ends all the same.
    const float cursor = refused.getCursor(night);
    refused.suspend();
    play(refused, 0.5F);
    refused.resume();
    refused.beginInterruption();
    play(refused, 0.5F);
    EXPECT_FLOAT_EQ(refused.getCursor(night), cursor);
    refused.endInterruption();
    EXPECT_FALSE(refused.isInterrupted());
    ASSERT_TRUE(settle(refused));
    EXPECT_FALSE(refused.isOutputAvailable());
    play(refused, 0.5F);
    EXPECT_NEAR(refused.getCursor(night), cursor + 0.5F, 0.01F);
    EXPECT_EQ(getWarnings().size(), 1U);
}

TEST_F(AudioOutputTest, OpensTheOutputOnceTheSystemOffersIt) {
    Mixer recovering({.device = true, .sampleRate = kRate, .channels = 2, .backend = &kBackend});
    const Mixer::VoiceId voice = recovering.play(makeTone(toFrames(0.25F)));
    ASSERT_TRUE(settle(recovering));
    play(recovering, 0.1F);

    // Ending an interruption, as the engine does whenever the app becomes active, opens the output, which then plays the voices.
    accepting = true;
    recovering.endInterruption();
    EXPECT_TRUE(recovering.isOutputOpening());
    ASSERT_TRUE(settle(recovering));
    EXPECT_TRUE(recovering.isOutputAvailable());
    play(recovering, 0.5F);
    EXPECT_TRUE(recovering.isActive(voice));
    EXPECT_NEAR(recovering.getCursor(voice), 0.1F, 0.01F);

    // An output that played and is refused at the end of an interruption warns again, and the voices go on without it.
    recovering.beginInterruption();
    accepting = false;
    recovering.endInterruption();
    EXPECT_FALSE(recovering.isInterrupted());
    ASSERT_TRUE(settle(recovering));
    EXPECT_FALSE(recovering.isOutputAvailable());
    EXPECT_EQ(getWarnings().size(), 2U);
    EXPECT_EQ(closedDevices, 1);
    play(recovering, 0.2F);
    EXPECT_FALSE(recovering.isActive(voice));
}

TEST_F(AudioOutputTest, KeepsPlayingWhileTheOutputDoesNotAnswer) {
    accepting = true;
    hang();
    const auto before = std::chrono::steady_clock::now();
    Mixer waiting({.device = true, .sampleRate = kRate, .channels = 2, .backend = &kBackend});
    EXPECT_LT(std::chrono::steady_clock::now() - before, std::chrono::seconds(1));
    EXPECT_TRUE(waiting.isOutputOpening());
    EXPECT_FALSE(waiting.isOutputAvailable());

    // The frames go on, and the voices move on without sound while the system keeps the opening waiting.
    const Mixer::VoiceId voice = waiting.play(makeTone(toFrames(8.0F)));
    play(waiting, Device::kAnswerSeconds - 0.5F);
    EXPECT_TRUE(getWarnings().empty());
    play(waiting, 1.0F);
    EXPECT_NEAR(waiting.getCursor(voice), Device::kAnswerSeconds + 0.5F, 0.02F);
    ASSERT_EQ(getWarnings().size(), 1U);
    EXPECT_NE(getWarnings().front().find("does not answer"), std::string::npos);

    // An interruption that ends meanwhile waits for the same opening, and the warning stays the only one.
    waiting.beginInterruption();
    waiting.endInterruption();
    play(waiting, 1.0F);
    EXPECT_TRUE(waiting.isOutputOpening());
    EXPECT_EQ(getWarnings().size(), 1U);

    // Once the system answers, the next update takes the device, which plays from then on.
    answer();
    ASSERT_TRUE(settle(waiting));
    EXPECT_TRUE(waiting.isOutputAvailable());
    EXPECT_EQ(getLines(core::Log::Level::Info).size(), 1U);
    EXPECT_EQ(getWarnings().size(), 1U);
}

TEST_F(AudioOutputTest, ClosesAnOpeningThatOutlivesItsMixer) {
    accepting = true;
    hang();
    {
        Mixer gone({.device = true, .sampleRate = kRate, .channels = 2, .backend = &kBackend});
        EXPECT_TRUE(gone.isOutputOpening());
    }

    // The thread that opened the device closes it once the system answers, since no mixer takes it anymore.
    answer();
    EXPECT_TRUE(waitFor([] { return closedDevices == 1; }));
}

TEST_F(AudioOutputTest, RunsAnAppWhoseOutputDoesNotAnswer) {
    accepting = true;
    hang();
    test::EngineFixture fixture({{"source/main.lua", "frames = 0 require('haylen.scene').push({update = function() frames = frames + 1 end})"}}, nullptr, {.audioOutput = &kBackend});
    EXPECT_EQ(fixture.lua("return require('haylen.audio').hasDevice()"), "true");
    EXPECT_EQ(fixture.lua("return require('haylen.audio').outputOpening()"), "true");

    // The app updates and draws its frames while the system keeps the device waiting, and plays sound once it answers.
    fixture.frames(240);
    EXPECT_EQ(fixture.lua("return frames"), "240");
    EXPECT_EQ(fixture.lua("return require('haylen.audio').outputAvailable()"), "false");
    EXPECT_EQ(getWarnings().size(), 1U);
    answer();
    EXPECT_TRUE(fixture.frameUntil([&fixture] { return fixture.lua("return require('haylen.audio').outputAvailable()") == "true"; }));
    EXPECT_EQ(fixture.lua("return require('haylen.audio').outputOpening()"), "false");
}

} // namespace haylen::audio
