#pragma once

#include <cstdint>
#include <vector>

namespace haylen::platform {

// Decodes one video file for `graphics::Video`, which the platform opens with its own decoder. Most decoders decode ahead of the playback on a worker of the task pool, one call at a time, and the video presents each frame at its time on its own clock. A decoder with a clock of its own, such as the video element of a browser, plays by itself on the frame thread instead: the video hands it play, pause, seek and rate, follows its time and shows the newest frame it decoded. Such a decoder may learn its size and its length after it opens, and reports a width of 0 until it knows them, while its reads of frames report its errors.
class VideoDecoder {
  public:
    struct Info {
        double duration = 0.0;
        int width = 0;
        int height = 0;

        // The audio of the video, with no channels for a video without sound.
        std::uint32_t sampleRate = 0;
        std::uint32_t channels = 0;
        bool clocked = false;
    };

    // A picture as RGBA8 rows without padding, with its time from the start of the video in seconds.
    struct Frame {
        std::vector<std::uint8_t> pixels;
        int width = 0;
        int height = 0;
        double time = 0.0;
    };

    enum class Read : std::uint8_t {
        Frame,
        Waiting,
        End,
    };

    virtual ~VideoDecoder() = default;

    [[nodiscard]] virtual const Info& getInfo() const noexcept = 0;

    // Decodes the next frame into `frame`, reusing its storage. A decoder with a clock of its own hands its newest frame once, and `Waiting` until another one shows.
    virtual Read readFrame(Frame& frame) = 0;

    // Appends the interleaved samples of the audio up to the time in seconds, and returns `false` once the audio ended. A decoder with a clock of its own appends what played since the last call.
    virtual bool readAudio(std::vector<float>& samples, double until) = 0;

    // Moves the decoding to the time in seconds, from where the next frame and the next samples come.
    virtual void seek(double seconds) = 0;

    // A decoder with a clock of its own plays or pauses at the rate, and reports the time it shows.
    virtual void control(bool, double) {}
    [[nodiscard]] virtual double getTime() const {
        return 0.0;
    }

  protected:
    VideoDecoder() = default;
};

} // namespace haylen::platform
