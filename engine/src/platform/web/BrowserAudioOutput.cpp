#include "platform/web/BrowserAudioOutput.hpp"
#include "platform/web/WebEntry.hpp"

#include <emscripten/emscripten.h>

#include <cstddef>
#include <span>
#include <stdexcept>
#include <utility>

// clang-format off
EM_JS(int, haylen_js_audio_supported, (), {
    return Module.haylen.mixer.supported() ? 1 : 0;
});

EM_JS(void, haylen_js_audio_open, (void* device, int channels, int sampleRate, int blockFrames, int bufferedFrames), {
    Module.haylen.mixer.open(device, channels, sampleRate, blockFrames, bufferedFrames);
});

EM_JS(void, haylen_js_audio_start, (void* device), {
    Module.haylen.mixer.start(device);
});

EM_JS(void, haylen_js_audio_stop, (void* device), {
    Module.haylen.mixer.stop(device);
});

EM_JS(int, haylen_js_audio_locked, (const void* device), {
    return Module.haylen.mixer.locked(device) ? 1 : 0;
});

EM_JS(void, haylen_js_audio_close, (void* device), {
    Module.haylen.mixer.close(device);
});
// clang-format on

namespace haylen::audio {

std::unique_ptr<port::AudioOutput::Playback> BrowserAudioOutput::open(const Request& request, Stream& stream) {
    if (haylen_js_audio_supported() == 0) {
        throw std::runtime_error(R"(Browsers offer "AudioWorklet" only to pages served over "https" or from "localhost".)");
    }

    const std::uint32_t bufferedFrames = request.sampleRate * kBufferedMilliseconds / 1000;
    return std::make_unique<PagePlayback>(stream, Format{.sampleRate = request.sampleRate, .channels = request.channels, .periodFrames = kBlockFrames, .periods = bufferedFrames / kBlockFrames}, bufferedFrames);
}

void BrowserAudioOutput::render(void* playback, float* samples, std::uint32_t frames) noexcept {
    static_cast<PagePlayback*>(playback)->render(samples, frames);
}

void BrowserAudioOutput::fail(void* playback, std::string reason) {
    static_cast<PagePlayback*>(playback)->fail(std::move(reason));
}

BrowserAudioOutput::PagePlayback::PagePlayback(Stream& source, const Format& value, std::uint32_t bufferedFrames) : stream(source), format(value) {
    haylen_js_audio_open(this, static_cast<int>(format.channels), static_cast<int>(format.sampleRate), static_cast<int>(format.periodFrames), static_cast<int>(bufferedFrames));
}

BrowserAudioOutput::PagePlayback::~PagePlayback() {
    haylen_js_audio_close(this);
}

void BrowserAudioOutput::PagePlayback::start() noexcept {
    haylen_js_audio_start(this);
}

void BrowserAudioOutput::PagePlayback::stop() noexcept {
    haylen_js_audio_stop(this);
}

bool BrowserAudioOutput::PagePlayback::isLocked() const noexcept {
    return haylen_js_audio_locked(this) != 0;
}

void BrowserAudioOutput::PagePlayback::render(float* samples, std::uint32_t frames) noexcept {
    stream.render(std::span<float>(samples, static_cast<std::size_t>(frames) * format.channels));
}

} // namespace haylen::audio

// Entry point for the mixer of the audio output, called through `Module.haylen.mixer` in `platform/web/haylen-runtime.js`.
extern "C" {

EMSCRIPTEN_KEEPALIVE void haylen_web_audio_render(void* playback, float* samples, int frames) {
    haylen::platform::WebEntry::guard("haylen_web_audio_render", [&] { haylen::audio::BrowserAudioOutput::render(playback, samples, static_cast<std::uint32_t>(frames)); });
}

// The page reports an output whose audio context or processor the browser refused, with the reason.
EMSCRIPTEN_KEEPALIVE void haylen_web_audio_failed(void* playback, const char* reason) {
    haylen::platform::WebEntry::guard("haylen_web_audio_failed", [&] { haylen::audio::BrowserAudioOutput::fail(playback, reason); });
}
}
