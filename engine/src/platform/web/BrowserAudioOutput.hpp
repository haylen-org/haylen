#pragma once

#include <miniaudio.h>

#include <string>
#include <unordered_map>

#include "audio/OutputBackend.hpp"

namespace haylen::audio {

// The audio output of the browser, the only backend miniaudio has there. The engine mixes on the thread of the module, the page or the worker of the Worker mode, whenever the mixer of `platform/web/haylen-runtime.js` asks for a block, and the mixer posts the blocks through a `MessageChannel` to the `AudioWorkletNode` that `platform/web/haylen-page.js` owns, whose processor in `platform/web/haylen-audio-worklet.js` plays them from a small ring buffer, so the output needs neither shared memory nor threads. The page keeps `kBufferedMilliseconds` of mixed audio ahead of the output, which a long frame of the app does not drain.
class BrowserAudioOutput final {
  public:
    // The backend the audio device plays through in browsers, which refuses to start when the browser offers no AudioWorklet.
    static const OutputBackend kBackend;

    BrowserAudioOutput() = delete;

    // Mixes the next frames of a started device into interleaved samples.
    static void render(ma_device* device, float* samples, ma_uint32 frames);

    // Records that the page could not play the output of a device, whose audio context or processor the browser refused after the device opened, which the device of the mixer takes at its next update.
    static void fail(ma_device* device, std::string reason);

  private:
    static constexpr ma_uint32 kBlockFrames = 256;
    static constexpr ma_uint32 kBufferedMilliseconds = 50;

    // Sets up a context whose playback devices play through the page. Fails when the browser offers no AudioWorklet, which it offers only to pages served over https or from localhost.
    static ma_result initContext(ma_context* context, const ma_context_config* config, ma_backend_callbacks* callbacks);
    static ma_result initDevice(ma_device* device, const ma_device_config* config, ma_device_descriptor* playback, ma_device_descriptor* capture);
    static ma_result uninitDevice(ma_device* device);
    static ma_result startDevice(ma_device* device);
    static ma_result stopDevice(ma_device* device);
    static bool isLocked(ma_device* device);
    [[nodiscard]] static const char* getFailure(ma_device* device);

    // The reasons of the outputs that failed, by their device, until the device closes.
    static std::unordered_map<ma_device*, std::string>& failures;
};

} // namespace haylen::audio
