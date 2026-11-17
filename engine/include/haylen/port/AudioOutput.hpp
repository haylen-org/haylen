#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "haylen/audio/OutputSettings.hpp"

namespace haylen::port {

// The audio output of a platform, which plays what the mixer of each app mixes. A port registers it where its platform plays sound, and the mixer of a port without it keeps time without sound, as a device that nobody hears would play. The platforms of the engine register one only where the audio library of the engine knows no output of the system, such as the page of a browser.
class AudioOutput {
  public:
    // The output that the mixer asks for: the samples of each second, the channels and how the output trades its delay for its power.
    struct Request {
        std::uint32_t sampleRate = 0;
        std::uint32_t channels = 0;
        audio::OutputSettings::Latency latency = audio::OutputSettings::Latency::Balanced;
    };

    // What an output plays, which the engine converts the mix to when it differs from the request, and the periods it keeps ahead of the speakers, which give the latency that apps read.
    struct Format {
        std::uint32_t sampleRate = 0;
        std::uint32_t channels = 0;
        std::uint32_t periodFrames = 0;
        std::uint32_t periods = 0;
    };

    // The mix of the engine, which the output reads between the start and the stop of its playback, from the one thread it plays on, such as the audio thread of the system or the run loop of a platform without threads. Each call fills the frames that the samples hold, interleaved 32-bit floats in the format of the output with the front left and front right channels first, and allocates nothing.
    class Stream {
      public:
        Stream() = default;
        Stream(const Stream&) = delete;
        Stream& operator=(const Stream&) = delete;
        virtual ~Stream() = default;

        virtual void render(std::span<float> samples) noexcept = 0;
    };

    // One opening of the output, which the mixer starts and stops on the frame thread and destroys when it closes the output. The start and the stop never fail, and an output that stops working after it opened says why through `getFailure`, which the frame thread asks once per frame.
    class Playback {
      public:
        Playback() = default;
        Playback(const Playback&) = delete;
        Playback& operator=(const Playback&) = delete;
        virtual ~Playback() = default;

        [[nodiscard]] virtual Format getFormat() const noexcept = 0;
        virtual void start() noexcept = 0;
        virtual void stop() noexcept = 0;

        // Whether the output waits for the person before it plays, where the platform keeps sound locked until a gesture.
        [[nodiscard]] virtual bool isLocked() const noexcept = 0;

        // The sentence that says why the output stopped working, which leaves the app without sound until the output opens again, or nothing while it works.
        [[nodiscard]] virtual std::optional<std::string> getFailure() const = 0;
    };

    AudioOutput() = default;
    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;
    virtual ~AudioOutput() = default;

    // Opens the output for the stream, which the playback reads until it is destroyed. The mixer opens it on a thread of its own, or on the frame thread where the platform has no other thread, and goes on without sound while the system takes its time, so it may block until the system answers. Throws `std::runtime_error` with the sentence that says why the platform refuses the output, which the log shows.
    [[nodiscard]] virtual std::unique_ptr<Playback> open(const Request& request, Stream& stream) = 0;
};

} // namespace haylen::port
