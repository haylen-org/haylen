#pragma once

#import <AVFoundation/AVFoundation.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/io/PackageReader.hpp"
#include "haylen/platform/VideoDecoder.hpp"
#import "platform/apple/HaylenVideoLoader.h"

namespace haylen::platform {

// Decodes a video file of the package with AVFoundation: an asset that reads the file through `HaylenVideoLoader`, and an asset reader that decodes its picture as BGRA, which turns into RGBA, and its sound as interleaved floats. A seek starts a new reader at the time.
class AppleVideoDecoder final : public VideoDecoder {
  public:
    // Opens the file and waits for its tracks and its duration. Throws `std::runtime_error` for a file that AVFoundation cannot play or that holds no picture.
    AppleVideoDecoder(std::shared_ptr<io::PackageReader> file, std::string_view path);
    ~AppleVideoDecoder() override;

    AppleVideoDecoder(const AppleVideoDecoder&) = delete;
    AppleVideoDecoder& operator=(const AppleVideoDecoder&) = delete;

    [[nodiscard]] const Info& getInfo() const noexcept override {
        return info;
    }
    Read readFrame(Frame& frame) override;
    bool readAudio(std::vector<float>& samples, double until) override;
    void seek(double seconds) override;

  private:
    void startReading(double seconds);
    [[noreturn]] void failReading() const;

    std::string path;
    Info info;
    HaylenVideoLoader* loader;
    dispatch_queue_t queue;
    AVURLAsset* asset;
    AVAssetTrack* videoTrack = nil;
    AVAssetTrack* audioTrack = nil;
    AVAssetReader* reader = nil;
    AVAssetReaderTrackOutput* videoOutput = nil;
    AVAssetReaderTrackOutput* audioOutput = nil;
    double soundTime = 0.0;
    bool soundEnded = false;
};

} // namespace haylen::platform
