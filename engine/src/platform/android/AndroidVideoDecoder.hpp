#pragma once

#include <media/NdkImageReader.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaDataSource.h>
#include <media/NdkMediaExtractor.h>
#include <miniaudio.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <vector>

#include "haylen/io/PackageReader.hpp"
#include "haylen/platform/VideoDecoder.hpp"
#include "platform/android/AndroidYuvImage.hpp"

namespace haylen::platform {

// Decodes a video file of the package with MediaExtractor and MediaCodec of the NDK, which read the file through a media data source over the package by range. The picture and the sound each have an extractor and a codec of their own, so they decode at their own pace. The picture decodes into the surface of an image reader, whose images describe their planes of YUV, which turn into RGBA, whatever layout the decoder of the device writes. The media data source needs Android 9, so earlier versions decode no video.
class AndroidVideoDecoder final : public VideoDecoder {
  public:
    // Opens the file and starts its codecs. Throws `std::runtime_error` for a file that Android cannot play or that holds no picture.
    AndroidVideoDecoder(std::shared_ptr<io::PackageReader> file, std::string_view path);
    ~AndroidVideoDecoder() override;

    AndroidVideoDecoder(const AndroidVideoDecoder&) = delete;
    AndroidVideoDecoder& operator=(const AndroidVideoDecoder&) = delete;

    // Whether this version of Android has the media data source that the decoder reads the package through.
    [[nodiscard]] static bool isAvailable() noexcept;

    [[nodiscard]] const Info& getInfo() const noexcept override {
        return info;
    }
    Read readFrame(Frame& frame) override;
    bool readAudio(std::vector<float>& samples, double until) override;
    void seek(double seconds) override;

  private:
    // One track with its extractor, its codec and the end of its input and output.
    struct Track {
        AMediaDataSource* source = nullptr;
        AMediaExtractor* extractor = nullptr;
        AMediaCodec* codec = nullptr;
        bool inputEnded = false;
        bool outputEnded = false;
    };

    // How long a read waits for the codec at most each time, and how many waits without output a read accepts before the codec counts as stuck. A picture that the codec rendered reaches the image reader shortly after.
    static constexpr std::int64_t kWaitMicroseconds = 10000;
    static constexpr int kPatience = 300;
    static constexpr int kImagePatience = 100;

    // The media data source and the extractor that reads from it came with Android 9, after the oldest version the engine runs on, so the decoder finds them in `libmediandk.so` when the first video opens.
    static AMediaDataSource* (*newSource)();
    static void (*setSourceData)(AMediaDataSource* source, void* data);
    static void (*setSourceReadAt)(AMediaDataSource* source, AMediaDataSourceReadAt callback);
    static void (*setSourceGetSize)(AMediaDataSource* source, AMediaDataSourceGetSize callback);
    static void (*setSourceClose)(AMediaDataSource* source, AMediaDataSourceClose callback);
    static void (*deleteSource)(AMediaDataSource* source);
    static media_status_t (*setExtractorSource)(AMediaExtractor* extractor, AMediaDataSource* source);
    [[nodiscard]] static bool load() noexcept;

    static ssize_t readAt(void* data, off64_t offset, void* buffer, size_t size);
    static ssize_t getSize(void* data);
    static void close(void* data);

    void open();

    // Opens the extractor of the first track whose MIME type starts with the prefix, and returns its format, or null without one.
    [[nodiscard]] AMediaFormat* openTrack(Track& track, std::string_view prefix);
    // Starts the codec of a track with its format, which it deletes.
    void startCodec(Track& track, AMediaFormat* format, bool picture);
    void feed(Track& track);
    // Takes the image that the codec rendered into the image reader and turns it into RGBA.
    void takeImage(Frame& frame);
    void release(Track& track) noexcept;
    [[noreturn]] void fail(std::string_view what) const;

    // Converts the decoded sound to the rate the video reported when it opened, for a codec whose output runs at another rate, as the sound of HE-AAC with an implicit SBR runs at twice the rate of its track.
    void setSoundRate(std::uint32_t rate);
    void appendSound(std::vector<float>& samples);

    std::string path;
    std::shared_ptr<io::PackageReader> file;
    Info info;
    Track picture;
    Track sound;
    AImageReader* images = nullptr;
    double skipUntil = 0.0;
    double soundTime = 0.0;
    std::uint32_t soundChannels = 0;
    std::uint32_t soundRate = 0;
    bool floatSound = false;
    ma_linear_resampler resampler{};
    bool resampling = false;
    std::vector<float> decodedSound;
    std::vector<float> resampledSound;
};

} // namespace haylen::platform
