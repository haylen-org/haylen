#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#include "audio/MixerFixture.hpp"
#include "haylen/audio/Mixer.hpp"
#include "haylen/platform/AudioStream.hpp"

namespace haylen::audio {

class AudioStreamTest : public MixerFixture {};

TEST_F(AudioStreamTest, PlaysAsAVoiceResampledAndSilentWhereSamplesAreMissing) {
    // A mono stream at half the rate of the mixer, which the voice resamples and spreads over both channels.
    auto stream = std::make_shared<platform::AudioStream>(kRate / 2, 1, platform::AudioStream::Format::Float32, kRate / 2);
    EXPECT_EQ(stream->push(std::vector<float>(toFrames(0.2F) / 2, 0.5F)), toFrames(0.2F) / 2);
    const Mixer::VoiceId voice = mixer.play(stream);
    const std::vector<float> left = renderLeft(toFrames(0.1F));
    EXPECT_NEAR(left[left.size() / 2], 0.5F, 0.01F);
    EXPECT_EQ(stream->getUnderrunCount(), 0U);

    // Once the samples run out the voice plays silence, counts underruns and keeps going.
    peak(toFrames(0.2F));
    EXPECT_GT(stream->getUnderrunCount(), 0U);
    EXPECT_EQ(peak(1024), 0.0F);
    mixer.update(1.0F / 60.0F);
    EXPECT_TRUE(mixer.isActive(voice)) << "A stream never ends by itself.";
    EXPECT_GT(mixer.getCursor(voice), 0.0F);

    stream->push(std::vector<float>(toFrames(0.1F) / 2, 0.25F));
    EXPECT_NEAR(settledPeak(1024), 0.25F, 0.01F);
    mixer.stop(voice);
    mixer.update(1.0F / 60.0F);
    EXPECT_FALSE(mixer.isActive(voice));

    EXPECT_THROW(mixer.play(stream, {.loop = true}), std::invalid_argument);
    EXPECT_THROW(mixer.play(stream, {.pitch = 2.0F}), std::invalid_argument);
    EXPECT_THROW(mixer.play(std::shared_ptr<platform::AudioStream>{}), std::invalid_argument);
}

TEST_F(AudioStreamTest, HandsTheStreamToTheNewestVoice) {
    auto stream = std::make_shared<platform::AudioStream>(kRate, 2, platform::AudioStream::Format::Int16, kRate);
    stream->push(std::vector<std::int16_t>(toFrames(1.0F) * 2, 16384));
    const Mixer::VoiceId first = mixer.play(stream, {.bus = "music"});
    EXPECT_NEAR(settledPeak(512), 0.5F, 0.01F);

    // The newer voice takes the stream over, so the older one plays silence instead of the samples meant for it.
    const Mixer::VoiceId second = mixer.play(stream, {.volume = 0.5F});
    EXPECT_NEAR(settledPeak(512), 0.25F, 0.01F);
    mixer.stop(second);
    EXPECT_EQ(settledPeak(512), 0.0F);
    EXPECT_TRUE(mixer.isActive(first));
}

TEST_F(AudioStreamTest, KeepsTheNewestSamplesAndDropsWhatDoesNotFit) {
    platform::AudioStream stream(8000, 2, platform::AudioStream::Format::Int16, 4);
    EXPECT_EQ(stream.push(std::vector<std::int16_t>{16384, -16384, 8192, -8192, 0, 0}), 3U);
    EXPECT_EQ(stream.push(std::vector<std::int16_t>{4096, -4096, 2048, -2048}), 1U) << "The ring holds four frames.";
    EXPECT_EQ(stream.getBufferedFrames(), 4U);

    std::vector<float> latest(6);
    EXPECT_EQ(stream.copyLatest(latest), 6U);
    EXPECT_EQ(latest, (std::vector<float>{0.25F, -0.25F, 0.0F, 0.0F, 0.125F, -0.125F}));

    // A reader without the latest token hears silence, and the one that claimed the stream reads what arrived and then silence.
    const std::uint64_t token = stream.claim();
    std::vector<float> output(10, 1.0F);
    stream.read(token - 1, output);
    EXPECT_EQ(output, std::vector<float>(10, 0.0F));
    stream.read(token, output);
    EXPECT_EQ(output, (std::vector<float>{0.5F, -0.5F, 0.25F, -0.25F, 0.0F, 0.0F, 0.125F, -0.125F, 0.0F, 0.0F}));
    EXPECT_EQ(stream.getUnderrunCount(), 1U);
    EXPECT_EQ(stream.getBufferedFrames(), 0U);

    EXPECT_THROW(stream.push(std::vector<float>{0.5F, 0.5F}), std::logic_error);
    EXPECT_THROW(stream.push(std::vector<std::int16_t>{1, 2, 3}), std::invalid_argument);
    EXPECT_THROW(platform::AudioStream(0, 2, platform::AudioStream::Format::Float32, 16), std::invalid_argument);
}

// A producer on another thread fills the ring while the mixer reads it, the way a microphone feeds a voice, so the thread sanitizer checks the handover.
TEST_F(AudioStreamTest, TakesSamplesFromAnotherThreadWhileTheMixerReads) {
    auto stream = std::make_shared<platform::AudioStream>(kRate, 1, platform::AudioStream::Format::Float32, 4096);
    mixer.play(stream);
    std::atomic<bool> done{false};
    // clang-format off
    std::thread producer([&stream, &done] {
        const std::vector<float> block(256, 0.5F);
        std::size_t written = 0;
        while (written < kRate) {
            written += stream->push(block);
            std::this_thread::yield();
        }
        done = true;
    });
    // clang-format on
    std::vector<float> latest(64);
    while (!done) {
        peak(256);
        stream->copyLatest(latest);
    }
    producer.join();
    EXPECT_EQ(latest.back(), 0.5F);
}

} // namespace haylen::audio
