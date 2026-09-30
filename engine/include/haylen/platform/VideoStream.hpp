#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

#include "haylen/core/Signal.hpp"
#include "haylen/graphics/Texture.hpp"

namespace haylen::graphics {
class Device;
}

namespace haylen::platform {

// Video frames that native code, such as a camera or a video decoder, pushes from any thread, and that the app draws as a texture. The stream keeps only the newest frame, and the engine uploads it into the texture at the start of a frame when it is new, so the texture changes once per frame at most. A frame of another size resizes the texture in place, so every handle to it stays current. Everything but push runs on the frame thread.
class VideoStream final {
  public:
    enum class Format : std::uint8_t {
        Rgba8,
        Bgra8,
    };

    // A stream opened with a size of 0 by 0 takes the size of its first frame. Throws std::invalid_argument for a negative size.
    VideoStream(Format format, int width, int height);

    [[nodiscard]] Format getFormat() const noexcept {
        return format;
    }

    // Copies a frame of width by height pixels in the format of the stream, whose rows start stride bytes apart, as RGBA8, from any thread. A newer frame replaces one that the app has not received yet. Throws std::invalid_argument for a size that is not positive or a stride shorter than a row.
    void push(const std::byte* pixels, int width, int height, std::size_t stride, double timestamp);

    // Uploads the newest frame into the texture when it is new, emits frameReceived and returns true, and returns false when no frame arrived since the last update.
    bool update(graphics::Device& device);

    // Returns the texture of the stream, created on the device the first time. Until the first frame it holds transparent pixels of the size the stream opened with, or a single one.
    [[nodiscard]] const graphics::Texture& getTexture(graphics::Device& device);

    // The size of the frame the texture shows, or the size the stream opened with before the first frame.
    [[nodiscard]] int getWidth() const noexcept {
        return shown.width;
    }
    [[nodiscard]] int getHeight() const noexcept {
        return shown.height;
    }

    // How many frames the app received, which leaves out the frames that newer ones replaced before an update.
    [[nodiscard]] std::uint64_t getFrameCount() const noexcept {
        return frameCount;
    }
    [[nodiscard]] double getTimestamp() const noexcept {
        return shown.timestamp;
    }

    // Lets go of the texture and the listeners when the engine that draws the stream stops. The newest frame stays for the next engine.
    void detach();

    // Receives the timestamp of every frame the app receives.
    core::Signal<double> frameReceived;

  private:
    // Video looks best filtered, so the texture of a stream samples linearly.
    static const graphics::Texture::Options kTextureOptions;

    struct Frame {
        std::vector<std::uint8_t> pixels;
        int width = 0;
        int height = 0;
        double timestamp = 0.0;
    };

    void createTexture(graphics::Device& device);

    const Format format;
    std::mutex mutex;

    // The producer fills the spare frame outside the lock and swaps it with the latest one, and update takes the latest one when it is fresh, so no frame allocates once the sizes settle.
    Frame latest;
    Frame spare;
    bool fresh = false;

    Frame shown;
    graphics::Texture texture;
    std::uint64_t frameCount = 0;
};

} // namespace haylen::platform
