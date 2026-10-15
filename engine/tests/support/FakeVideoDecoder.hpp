#pragma once

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "haylen/platform/VideoDecoder.hpp"

namespace haylen::test {

// A decoder that makes a video of solid frames, whose red channel counts the frames from 0, and of a constant sound, the way the decoder of a platform decodes a file. A test reads what the video asked of it from its log, which the workers write.
class FakeVideoDecoder final : public platform::VideoDecoder {
  public:
    struct Log {
        std::mutex mutex;
        std::vector<double> seeks;
        std::vector<std::string> controls;
        int framesRead = 0;
        double clock = 0.0;
        bool frameShown = false;
        std::set<std::thread::id> readingThreads;

        // Whether a decoder with a clock of its own knows its size yet, and the error its next read of a frame throws.
        bool sized = true;
        std::string error;

        // Holds every read of a frame while it is set, so a test sees the playback of a decoder that falls behind.
        bool held = false;
        std::condition_variable released;
    };

    struct Settings {
        Info info{.duration = 1.0, .width = 4, .height = 2, .sampleRate = 8000, .channels = 2};
        double frameRate = 10.0;
        float sample = 0.25F;

        // Fails the read of this frame, to show what a file that breaks while it plays does.
        std::optional<int> failAt;
    };

    FakeVideoDecoder(Settings settings, std::shared_ptr<Log> log);

    [[nodiscard]] const Info& getInfo() const noexcept override;
    Read readFrame(Frame& frame) override;
    bool readAudio(std::vector<float>& samples, double until) override;
    void seek(double seconds) override;
    void control(bool playing, double rate) override;
    [[nodiscard]] double getTime() const override;

  private:
    Settings settings;
    mutable Info shown;
    std::shared_ptr<Log> log;
    int next = 0;
    double soundTime = 0.0;
};

} // namespace haylen::test
