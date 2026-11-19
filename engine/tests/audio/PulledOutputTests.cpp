#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "audio/MixerFixture.hpp"
#include "haylen/port/AudioOutput.hpp"
#include "io/MemoryReader.hpp"
#include "support/AudioFiles.hpp"

namespace haylen::audio {

// Plays through an audio output whose playback reads the mix on a thread of its own, the way the audio thread of a platform does, so the tests see what the frame thread may touch while the output reads.
class PulledOutputTest : public MixerFixture {
  protected:
    class PulledPlayback final : public port::AudioOutput::Playback {
      public:
        PulledPlayback(const port::AudioOutput::Request& request, port::AudioOutput::Stream& source) noexcept : format{.sampleRate = request.sampleRate, .channels = request.channels, .periodFrames = kPeriod, .periods = 2}, stream(source) {}

        ~PulledPlayback() override {
            stop();
        }

        PulledPlayback(const PulledPlayback&) = delete;
        PulledPlayback& operator=(const PulledPlayback&) = delete;

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
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        port::AudioOutput::Format format;
        port::AudioOutput::Stream& stream;
        std::atomic<bool> running = false;
        std::thread reader;
    };

    class PulledOutput final : public port::AudioOutput {
      public:
        [[nodiscard]] std::unique_ptr<Playback> open(const Request& request, Stream& stream) override {
            return std::make_unique<PulledPlayback>(request, stream);
        }
    };

    static constexpr std::uint32_t kPeriod = 256;

    std::shared_ptr<port::AudioOutput> output = std::make_shared<PulledOutput>();
};

TEST_F(PulledOutputTest, MovesTheCursorOfStreamedVoicesWhileTheDeviceReadsThem) {
    Mixer pulled({.device = true, .sampleRate = kRate, .channels = 2, .output = output}, jobs);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (pulled.isOutputOpening() && std::chrono::steady_clock::now() < deadline) {
        pulled.update(0.0F);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    ASSERT_TRUE(pulled.isOutputAvailable());

    for (const std::string name : {"tone.ogg", "tone.mp3"}) {
        const Sound sound = Sound::stream(std::make_shared<io::MemoryReader>(std::make_shared<const std::vector<std::uint8_t>>(test::AudioFiles::read(name))));
        EXPECT_NEAR(sound.getDuration(), 2.0F, 0.1F) << name;
        const Mixer::VoiceId voice = pulled.play(sound, {.loop = true});
        for (int step = 0; step < 40; ++step) {
            pulled.setCursor(voice, static_cast<float>(step % 4) * 0.5F);
            pulled.update(1.0F / 60.0F);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }

        pulled.setCursor(voice, 1000.0F);
        EXPECT_LE(pulled.getCursor(voice), sound.getDuration() + 0.01F) << name;
        pulled.stop(voice);
    }
}

} // namespace haylen::audio
