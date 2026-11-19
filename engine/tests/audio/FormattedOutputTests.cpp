#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "audio/MixerFixture.hpp"
#include "haylen/port/AudioOutput.hpp"

namespace haylen::audio {

// Reads the mix in the format of an output that plays mono at another rate, whenever the test asks, as a port that plays from its own loop does.
class FormattedOutputTest : public MixerFixture {
  protected:
    class FormattedPlayback final : public port::AudioOutput::Playback {
      public:
        explicit FormattedPlayback(port::AudioOutput::Stream& source) noexcept : stream(source) {}

        FormattedPlayback(const FormattedPlayback&) = delete;
        FormattedPlayback& operator=(const FormattedPlayback&) = delete;

        [[nodiscard]] port::AudioOutput::Format getFormat() const noexcept override {
            return {.sampleRate = 44100, .channels = 1, .periodFrames = 441, .periods = 4};
        }

        void start() noexcept override {
            started = true;
        }

        void stop() noexcept override {
            started = false;
        }

        [[nodiscard]] bool isLocked() const noexcept override {
            return false;
        }

        [[nodiscard]] std::optional<std::string> getFailure() const override {
            return std::nullopt;
        }

        port::AudioOutput::Stream& stream;
        bool started = false;
    };

    class FormattedOutput final : public port::AudioOutput {
      public:
        [[nodiscard]] std::unique_ptr<Playback> open(const Request& request, Stream& stream) override {
            EXPECT_EQ(request.sampleRate, kRate);
            EXPECT_EQ(request.channels, 2U);
            auto opened = std::make_unique<FormattedPlayback>(stream);
            playback = opened.get();
            return opened;
        }

        FormattedPlayback* playback = nullptr;
    };

    std::shared_ptr<FormattedOutput> output = std::make_shared<FormattedOutput>();
};

TEST_F(FormattedOutputTest, PlaysTheMixInTheFormatOfTheOutput) {
    Mixer formatted({.device = true, .sampleRate = kRate, .channels = 2, .output = output}, jobs);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (formatted.isOutputOpening() && std::chrono::steady_clock::now() < deadline) {
        formatted.update(0.0F);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    ASSERT_TRUE(formatted.isOutputAvailable());
    ASSERT_NE(output->playback, nullptr);
    EXPECT_TRUE(output->playback->started);
    EXPECT_NEAR(formatted.getOutputLatency(), 0.04F, 1e-6F);

    // The output reads one sample for each frame of its own format, which the mixer converts the tone to.
    formatted.play(makeTone(toFrames(1.0F)));
    std::vector<float> samples(441);
    output->playback->stream.render(samples);
    EXPECT_GT(*std::ranges::max_element(samples), 0.1F);
}

} // namespace haylen::audio
