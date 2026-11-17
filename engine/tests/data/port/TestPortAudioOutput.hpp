#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "haylen/port/AudioOutput.hpp"

namespace haylen::platform {

// The audio output of the test port, which hears nothing: the run loop of the port reads one period of the mix after each frame, as a platform that plays sound from its own loop does, and counts the frames it read. It plays at its own rate and with its own period, so the mixer converts the mix and reports the latency of this output.
class TestPortAudioOutput final : public port::AudioOutput {
  public:
    static constexpr std::uint32_t kSampleRate = 44100;
    static constexpr std::uint32_t kChannels = 2;
    static constexpr std::uint32_t kPeriodFrames = 735;
    static constexpr std::uint32_t kPeriods = 3;

    [[nodiscard]] std::unique_ptr<Playback> open(const Request& request, Stream& stream) override;

    // Reads one period of the mix of the playback that plays, on the frame thread.
    void play();

    [[nodiscard]] std::uint64_t getPlayedFrames() const noexcept {
        return playedFrames;
    }

  private:
    // The one playback of the output, which the mixer starts and stops on the frame thread.
    class TestPortPlayback final : public Playback {
      public:
        TestPortPlayback(TestPortAudioOutput& value, Stream& source) noexcept : owner(value), stream(source) {}
        ~TestPortPlayback() override;

        TestPortPlayback(const TestPortPlayback&) = delete;
        TestPortPlayback& operator=(const TestPortPlayback&) = delete;

        [[nodiscard]] Format getFormat() const noexcept override;
        void start() noexcept override;
        void stop() noexcept override;
        [[nodiscard]] bool isLocked() const noexcept override;
        [[nodiscard]] std::optional<std::string> getFailure() const override;

        void render(std::vector<float>& samples) noexcept;

      private:
        TestPortAudioOutput& owner;
        Stream& stream;
    };

    std::vector<float> samples = std::vector<float>(static_cast<std::size_t>(kPeriodFrames) * kChannels);
    TestPortPlayback* playing = nullptr;
    std::uint64_t playedFrames = 0;
};

} // namespace haylen::platform
