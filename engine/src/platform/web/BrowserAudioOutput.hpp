#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "haylen/port/AudioOutput.hpp"

namespace haylen::audio {

// The audio output of the browser, which the web platform registers as its audio service, since the audio library of the engine knows no output of the system there. The engine mixes on the thread of the module, the page or the worker of the Worker mode, whenever the mixer of `platform/web/haylen-runtime.js` asks for a block, and the mixer posts the blocks through a `MessageChannel` to the `AudioWorkletNode` that `platform/web/haylen-page.js` owns, whose processor in `platform/web/haylen-audio-worklet.js` plays them from a small ring buffer, so the output needs neither shared memory nor threads. The page keeps `kBufferedMilliseconds` of mixed audio ahead of the output, which a long frame of the app does not drain.
class BrowserAudioOutput final : public port::AudioOutput {
  public:
    // Opens an output of the page at the sample rate of the mixer, which the browser converts to the rate of the hardware. Throws `std::runtime_error` when the browser offers no AudioWorklet, which it offers only to pages served over https or from localhost.
    [[nodiscard]] std::unique_ptr<Playback> open(const Request& request, Stream& stream) override;

    // Mixes the next frames of a playback into interleaved samples, as the mixer of the page asks.
    static void render(void* playback, float* samples, std::uint32_t frames) noexcept;

    // Records that the page could not play a playback, whose audio context or processor the browser refused after it opened, which the mixer of the engine takes at its next update.
    static void fail(void* playback, std::string reason);

  private:
    static constexpr std::uint32_t kBlockFrames = 256;
    static constexpr std::uint32_t kBufferedMilliseconds = 50;

    // One output of the page, which the page knows by its address and keeps the buffered frames ahead of.
    class PagePlayback final : public Playback {
      public:
        PagePlayback(Stream& source, const Format& value, std::uint32_t bufferedFrames);
        ~PagePlayback() override;

        PagePlayback(const PagePlayback&) = delete;
        PagePlayback& operator=(const PagePlayback&) = delete;

        [[nodiscard]] Format getFormat() const noexcept override {
            return format;
        }
        void start() noexcept override;
        void stop() noexcept override;
        [[nodiscard]] bool isLocked() const noexcept override;
        [[nodiscard]] std::optional<std::string> getFailure() const override {
            return failure;
        }

        void render(float* samples, std::uint32_t frames) noexcept;

        void fail(std::string reason) {
            failure = std::move(reason);
        }

      private:
        Stream& stream;
        Format format;
        std::optional<std::string> failure;
    };
};

} // namespace haylen::audio
