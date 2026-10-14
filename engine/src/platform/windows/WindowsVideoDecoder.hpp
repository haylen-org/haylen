#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/io/PackageReader.hpp"
#include "haylen/platform/VideoDecoder.hpp"

namespace haylen::platform {

// Decodes a video file of the package with the source reader of Media Foundation, which reads the file through a `WindowsPackageStream` and converts the picture into 32-bit RGB, which turns into RGBA, and the sound into interleaved floats. Media Foundation and COM start on every thread that uses the decoder, the workers of the pools included.
class WindowsVideoDecoder final : public VideoDecoder {
  public:
    // Opens the file. Throws `std::runtime_error` for a file that Media Foundation cannot play or that holds no picture.
    WindowsVideoDecoder(std::shared_ptr<io::PackageReader> file, std::string_view path);
    ~WindowsVideoDecoder() override;

    WindowsVideoDecoder(const WindowsVideoDecoder&) = delete;
    WindowsVideoDecoder& operator=(const WindowsVideoDecoder&) = delete;

    [[nodiscard]] const Info& getInfo() const noexcept override {
        return info;
    }
    Read readFrame(Frame& frame) override;
    bool readAudio(std::vector<float>& samples, double until) override;
    void seek(double seconds) override;

  private:
    // Media Foundation counts time in units of 100 nanoseconds.
    static constexpr double kUnitsPerSecond = 10000000.0;

    static void enterThread();
    void check(HRESULT result, std::string_view what) const;
    void chooseFormats();
    void readPictureFormat();

    std::string path;
    Info info;
    Microsoft::WRL::ComPtr<IMFSourceReader> reader;
    std::uint32_t soundChannels = 0;
    long stride = 0;
    double soundTime = 0.0;

    // A seek lands on the key frame before the time it asks for, and the reads drop what comes before that time.
    double seekTarget = 0.0;
    bool soundEnded = false;
};

} // namespace haylen::platform
