#pragma once

#include <memory>
#include <string_view>

#include "haylen/platform/VideoDecoder.hpp"

namespace haylen::io {
class PackageReader;
}

namespace haylen::port {

// The decoder of video files of a platform. A port registers it where its platform decodes video, and the engine fails every video of a platform without it with a sentence that says the platform has no decoder.
class VideoService {
  public:
    VideoService() = default;
    VideoService(const VideoService&) = delete;
    VideoService& operator=(const VideoService&) = delete;
    virtual ~VideoService() = default;

    // Opens a decoder of the video file that the reader reads, on a worker of the I/O pool, so several may open at once. Throws `std::runtime_error` for a file the platform cannot decode.
    [[nodiscard]] virtual std::unique_ptr<platform::VideoDecoder> openVideo(std::shared_ptr<io::PackageReader> file, std::string_view path) = 0;
};

} // namespace haylen::port
