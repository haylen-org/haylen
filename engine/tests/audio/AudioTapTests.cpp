#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "audio/MixerFixture.hpp"
#include "haylen/audio/Tap.hpp"
#include "haylen/port/AudioOutput.hpp"
#include "support/AllocationTracker.hpp"

namespace haylen::audio {

// Opens taps on the master bus of a mixer that renders on demand, and of mixers whose output plays on its own, and compares what the taps hear with what the output plays.
class AudioTapTest : public MixerFixture {
  protected:
    // A playback that reads the mix on a thread of its own, the way the audio thread of a platform does, and keeps everything it played.
    class RecordingPlayback final : public port::AudioOutput::Playback {
      public:
        RecordingPlayback(const port::AudioOutput::Request& request, port::AudioOutput::Stream& source, std::vector<float>& record, std::mutex& lock) noexcept : format{.sampleRate = request.sampleRate, .channels = request.channels, .periodFrames = kPeriod, .periods = 2}, stream(source), played(record), mutex(lock) {}

        ~RecordingPlayback() override {
            stop();
        }

        RecordingPlayback(const RecordingPlayback&) = delete;
        RecordingPlayback& operator=(const RecordingPlayback&) = delete;

        [[nodiscard]] port::AudioOutput::Format getFormat() const noexcept override {
            return format;
        }

        void start() noexcept override {
            running = true;
            reader = std::thread([this] { pull(); });
        }

        void stop() noexcept override {
            running = false;
            if (reader.joinable()) {
                reader.join();
            }
        }

        [[nodiscard]] bool isLocked() const noexcept override {
            return false;
        }

        [[nodiscard]] std::optional<std::string> getFailure() const override {
            return std::nullopt;
        }

      private:
        void pull() {
            std::vector<float> samples(static_cast<std::size_t>(kPeriod) * format.channels);
            while (running) {
                stream.render(samples);
                {
                    const std::scoped_lock hold(mutex);
                    played.insert(played.end(), samples.begin(), samples.end());
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        port::AudioOutput::Format format;
        port::AudioOutput::Stream& stream;
        std::vector<float>& played;
        std::mutex& mutex;
        std::atomic<bool> running = false;
        std::thread reader;
    };

    class RecordingOutput final : public port::AudioOutput {
      public:
        RecordingOutput(std::vector<float>& record, std::mutex& lock) noexcept : played(record), mutex(lock) {}

        [[nodiscard]] std::unique_ptr<Playback> open(const Request& request, Stream& stream) override {
            return std::make_unique<RecordingPlayback>(request, stream, played, mutex);
        }

      private:
        std::vector<float>& played;
        std::mutex& mutex;
    };

    static constexpr std::uint32_t kPeriod = 256;

    // Reads every frame that waits in the tap.
    [[nodiscard]] static std::vector<float> drain(Tap& tap) {
        std::vector<float> samples(tap.getAvailableFrames() * tap.getChannels());
        const std::size_t frames = tap.read(samples);
        samples.resize(frames * tap.getChannels());
        return samples;
    }

    std::mutex playedMutex;
    std::vector<float> played;
};

TEST_F(AudioTapTest, HearsExactlyWhatTheMixRendersOnEveryTap) {
    const std::shared_ptr<Tap> first = mixer.tapMaster(4096);
    const std::shared_ptr<Tap> second = mixer.tapMaster(4096);
    EXPECT_EQ(first->getSampleRate(), kRate);
    EXPECT_EQ(first->getChannels(), 2U);
    EXPECT_EQ(first->getCapacity(), 4096U);

    mixer.play(makeSine(440.0F));
    std::vector<float> rendered(1024 * 2);
    mixer.render(rendered);

    EXPECT_EQ(first->getAvailableFrames(), 1024U);
    EXPECT_EQ(drain(*first), rendered);
    EXPECT_EQ(drain(*second), rendered);
    EXPECT_EQ(first->getAvailableFrames(), 0U);
    EXPECT_EQ(first->getDroppedFrames(), 0U);

    // A read into fewer samples than wait moves whole frames, the oldest first.
    mixer.render(rendered);
    std::vector<float> part(301);
    EXPECT_EQ(first->read(part), 150U);
    EXPECT_TRUE(std::equal(part.begin(), part.begin() + 300, rendered.begin()));
    EXPECT_EQ(first->getAvailableFrames(), 1024U - 150U);
}

// The output mute silences the tap too, since the tap hears what the output plays.
TEST_F(AudioTapTest, HearsTheMuteOfTheOutput) {
    mixer.play(makeTone(toFrames(1.0F)));
    mixer.setOutputMuted(true);
    peak(2048);
    const std::shared_ptr<Tap> tap = mixer.tapMaster(8192);
    EXPECT_EQ(peak(512), 0.0F);
    const std::vector<float> muted = drain(*tap);
    ASSERT_EQ(muted.size(), 512U * 2U);
    EXPECT_TRUE(std::ranges::all_of(muted, [](float sample) { return sample == 0.0F; }));
}

TEST_F(AudioTapTest, DropsAndCountsTheFramesThatDoNotFit) {
    const std::shared_ptr<Tap> tap = mixer.tapMaster(256);
    mixer.play(makeTone(toFrames(1.0F)));
    std::vector<float> rendered(1000 * 2);
    mixer.render(rendered);

    EXPECT_EQ(tap->getAvailableFrames(), 256U);
    EXPECT_EQ(tap->getDroppedFrames(), 1000U - 256U);
    const std::vector<float> kept = drain(*tap);
    EXPECT_TRUE(std::equal(kept.begin(), kept.end(), rendered.begin()));

    mixer.render(std::span<float>(rendered.data(), 100 * 2));
    EXPECT_EQ(tap->getAvailableFrames(), 100U);
    EXPECT_EQ(tap->getDroppedFrames(), 1000U - 256U);
}

// A closed tap keeps the frames that wait in it and hears nothing more, and a tap that nothing but the mixer holds closes at the next update, which frees its place.
TEST_F(AudioTapTest, ClosesOnRequestAndOnceNothingHoldsIt) {
    const std::shared_ptr<Tap> tap = mixer.tapMaster(1024);
    std::vector<float> rendered(64 * 2);
    mixer.render(rendered);
    tap->close();
    EXPECT_FALSE(tap->isOpen());
    tap->close();
    mixer.render(rendered);
    EXPECT_EQ(tap->getAvailableFrames(), 64U);

    std::vector<std::shared_ptr<Tap>> taps;
    for (std::size_t index = 0; index < Mixer::kMaxTaps; ++index) {
        taps.push_back(mixer.tapMaster(16));
    }

    EXPECT_THROW((void)mixer.tapMaster(16), std::length_error);
    EXPECT_THROW((void)mixer.tapMaster(0), std::invalid_argument);

    const std::weak_ptr<Tap> released = taps.back();
    taps.pop_back();
    EXPECT_FALSE(released.expired());
    mixer.update(0.0F);
    EXPECT_TRUE(released.expired());
    EXPECT_NE(mixer.tapMaster(16), nullptr);
}

TEST_F(AudioTapTest, ClosesWhenTheMixerGoes) {
    std::shared_ptr<Tap> tap;
    {
        Mixer local({.device = false, .sampleRate = kRate, .channels = 2, .maxVoices = 4}, jobs);
        tap = local.tapMaster(64);
        std::vector<float> rendered(32 * 2);
        local.render(rendered);
    }

    EXPECT_FALSE(tap->isOpen());
    EXPECT_EQ(tap->getAvailableFrames(), 32U);
    tap->close();
}

// Rendering into the taps and reading them allocates nothing once the mixer mixed its first blocks.
TEST_F(AudioTapTest, MixesIntoTapsAndReadsThemWithoutAllocating) {
    const std::shared_ptr<Tap> tap = mixer.tapMaster(4096);
    mixer.play(makeSine(220.0F), {.loop = true});
    std::vector<float> rendered(512 * 2);
    std::vector<float> read(512 * 2);
    for (int block = 0; block < 4; ++block) {
        mixer.render(rendered);
        tap->read(read);
    }

    std::size_t frames = 0;
    std::size_t allocated = 0;
    {
        const test::AllocationTracker tracker;
        for (int block = 0; block < 64; ++block) {
            mixer.render(rendered);
            frames += tap->read(read);
        }

        allocated = tracker.getTotal();
    }

    EXPECT_EQ(allocated, 0U);
    EXPECT_EQ(frames, 64U * 512U);
    EXPECT_EQ(read, rendered);
}

// A mixer that keeps time without a device mixes the time of each update, and the tap hears it.
TEST_F(AudioTapTest, HearsTheTimeThatAMixerWithoutSoundKeeps) {
    Mixer timed({.device = false, .keepsTime = true, .sampleRate = kRate, .channels = 2, .maxVoices = 4}, jobs);
    const std::shared_ptr<Tap> tap = timed.tapMaster(kRate);
    timed.play(makeTone(toFrames(1.0F)));
    timed.update(0.1F);
    EXPECT_EQ(tap->getAvailableFrames(), static_cast<std::size_t>(kRate / 10));
    const std::vector<float> heard = drain(*tap);
    EXPECT_NEAR(*std::ranges::max_element(heard), 0.5F, 0.01F);
}

// The output of a platform that the host gives the mixer, such as the page of a browser or a console, reads the mix on its own thread, and the tap hears the same frames in the same order.
TEST_F(AudioTapTest, HearsWhatTheOutputOfThePlatformPlays) {
    std::vector<float> heard;
    {
        Mixer pulled({.device = true, .sampleRate = kRate, .channels = 2, .output = std::make_shared<RecordingOutput>(played, playedMutex)}, jobs);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (pulled.isOutputOpening() && std::chrono::steady_clock::now() < deadline) {
            pulled.update(0.0F);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        ASSERT_TRUE(pulled.isOutputAvailable());
        pulled.play(Sound::decode(makePositions(kRate * 4)), {.bus = "music"});
        const std::shared_ptr<Tap> tap = pulled.tapMaster(kRate);
        while (tap->getAvailableFrames() < kRate / 10 && std::chrono::steady_clock::now() < deadline) {
            pulled.update(0.0F);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        ASSERT_GE(tap->getAvailableFrames(), kRate / 10);
        heard = drain(*tap);
    }

    // The tap hears each block when the mix makes it, and the output plays it in periods of its own afterwards, so the comparison waits until the mixer closed its output, whose thread recorded every period it played. From the point where the tap opened, the record then runs frame by frame inside what the tap heard, up to the part of the last block that the output still held when it closed.
    const std::scoped_lock hold(playedMutex);
    const auto half = heard.begin() + static_cast<std::ptrdiff_t>(heard.size() / 2);
    const auto start = std::search(played.begin(), played.end(), heard.begin(), half);
    ASSERT_NE(start, played.end());
    const auto recorded = std::min(played.end() - start, std::ssize(heard));
    EXPECT_TRUE(std::equal(start, start + recorded, heard.begin()));
}

} // namespace haylen::audio
