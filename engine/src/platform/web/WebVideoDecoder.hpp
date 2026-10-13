#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/io/PackageReader.hpp"
#include "haylen/platform/VideoDecoder.hpp"

namespace haylen::platform {

// Plays a video file of the package in a video element of the page, which `haylen.video` of `haylen-runtime.js` makes from the bytes of the file. The element keeps its own clock, so the decoder hands it play, pause, seek and rate, reports its time and its newest picture, and passes on the sound that the page took from the element. The element learns its size and its length after it opens, so the decoder reports them once the page knows them.
class WebVideoDecoder final : public VideoDecoder {
  public:
    // Hands the bytes of the file to the page in pieces. Throws `std::runtime_error` for a file the package cannot read.
    WebVideoDecoder(io::PackageReader& file, std::string_view path);
    ~WebVideoDecoder() override;

    WebVideoDecoder(const WebVideoDecoder&) = delete;
    WebVideoDecoder& operator=(const WebVideoDecoder&) = delete;

    [[nodiscard]] const Info& getInfo() const noexcept override;
    Read readFrame(Frame& frame) override;
    bool readAudio(std::vector<float>& samples, double until) override;
    void seek(double seconds) override;
    void control(bool playing, double rate) override;
    [[nodiscard]] double getTime() const override;

  private:
    // The pieces of the file that reach the page, and the samples that a read of the sound takes at most at once.
    static constexpr std::size_t kPieceSize = 1U << 20U;
    static constexpr std::size_t kSoundBlock = 4096;

    // Reads what the page knows of the element until it knows its size, and throws the error of the element.
    void refresh() const;

    std::string path;
    int id = 0;
    mutable Info info{.clocked = true};
    mutable std::string error;
    std::vector<float> sound;
};

} // namespace haylen::platform
