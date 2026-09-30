#include <gtest/gtest.h>
#include <miniaudio.h>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "audio/MixerFixture.hpp"
#include "audio/OutputBackend.hpp"
#include "haylen/core/Log.hpp"

namespace haylen::audio {

// Plays through a backend that refuses the audio until a test lets it through, the way a browser without `AudioWorklet` or an iOS audio session that another app holds refuses it. The device it opens never asks for samples.
class AudioOutputTest : public MixerFixture {
  protected:
    static constexpr float kFrameSeconds = 1.0F / 60.0F;

    void SetUp() override {
        accepting = false;
        listener = core::Log::addListener([this](core::Log::Level level, std::string_view line) { lines.emplace_back(level, std::string(line)); });
    }

    void TearDown() override {
        core::Log::removeListener(listener);
    }

    static ma_result initContext(ma_context*, const ma_context_config*, ma_backend_callbacks* callbacks) {
        if (!accepting) {
            return MA_FAILED_TO_INIT_BACKEND;
        }
        callbacks->onDeviceInit = &initDevice;
        callbacks->onDeviceUninit = &ignore;
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

    static ma_result ignore(ma_device*) {
        return MA_SUCCESS;
    }

    // Updates the mixer frame by frame over the seconds, as the engine does.
    static void play(Mixer& target, float seconds) {
        for (float elapsed = 0.0F; elapsed + kFrameSeconds / 2.0F < seconds; elapsed += kFrameSeconds) {
            target.update(kFrameSeconds);
        }
    }

    [[nodiscard]] std::vector<std::string> getWarnings() const {
        std::vector<std::string> warnings;
        for (const auto& [level, line] : lines) {
            if (level == core::Log::Level::Warning) {
                warnings.push_back(line);
            }
        }
        return warnings;
    }

    static inline bool accepting = false;
    static constexpr OutputBackend kBackend{.initContext = &initContext, .refusal = "the test refuses the audio"};

    std::vector<std::pair<core::Log::Level, std::string>> lines;
    std::uint64_t listener = 0;
};

TEST_F(AudioOutputTest, KeepsTimeWithoutSoundWhileTheSystemRefusesTheOutput) {
    Mixer refused({.device = true, .sampleRate = kRate, .channels = 2, .backend = &kBackend});
    EXPECT_TRUE(refused.hasDevice());
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
    EXPECT_FALSE(refused.isOutputAvailable());
    play(refused, 0.5F);
    EXPECT_NEAR(refused.getCursor(night), cursor + 0.5F, 0.01F);
    EXPECT_EQ(getWarnings().size(), 1U);
}

TEST_F(AudioOutputTest, OpensTheOutputOnceTheSystemOffersIt) {
    Mixer recovering({.device = true, .sampleRate = kRate, .channels = 2, .backend = &kBackend});
    const Mixer::VoiceId voice = recovering.play(makeTone(toFrames(0.25F)));
    play(recovering, 0.1F);

    // Ending an interruption, as the engine does whenever the app becomes active, opens the output, which then plays the voices.
    accepting = true;
    recovering.endInterruption();
    EXPECT_TRUE(recovering.isOutputAvailable());
    play(recovering, 0.5F);
    EXPECT_TRUE(recovering.isActive(voice));
    EXPECT_NEAR(recovering.getCursor(voice), 0.1F, 0.01F);

    // An output that played and is refused at the end of an interruption warns again, and the voices go on without it.
    recovering.beginInterruption();
    accepting = false;
    recovering.endInterruption();
    EXPECT_FALSE(recovering.isInterrupted());
    EXPECT_FALSE(recovering.isOutputAvailable());
    EXPECT_EQ(getWarnings().size(), 2U);
    play(recovering, 0.2F);
    EXPECT_FALSE(recovering.isActive(voice));
}

} // namespace haylen::audio
