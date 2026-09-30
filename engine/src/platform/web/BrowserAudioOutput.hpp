#pragma once

#include <miniaudio.h>

#include "audio/OutputBackend.hpp"

namespace haylen::audio {

// The audio output of the page, the only backend miniaudio has in the browser. The engine mixes on the page thread whenever `platform/web/haylen-runtime.js` asks for a block, and the page posts the blocks to an `AudioWorkletNode`, whose processor in `platform/web/haylen-audio-worklet.js` plays them from a small ring buffer, so the output needs neither shared memory nor threads. The page keeps `kBufferedMilliseconds` of mixed audio ahead of the output, which a long frame of the app does not drain.
class BrowserAudioOutput final {
  public:
    // The backend the audio device plays through in browsers, which refuses to start when the browser offers no AudioWorklet.
    static const OutputBackend kBackend;

    BrowserAudioOutput() = delete;

    // Mixes the next frames of a started device into interleaved samples.
    static void render(ma_device* device, float* samples, ma_uint32 frames);

  private:
    static constexpr ma_uint32 kBlockFrames = 256;
    static constexpr ma_uint32 kBufferedMilliseconds = 50;

    // Sets up a context whose playback devices play through the page. Fails when the browser offers no AudioWorklet, which it offers only to pages served over https or from localhost.
    static ma_result initContext(ma_context* context, const ma_context_config* config, ma_backend_callbacks* callbacks);
    static ma_result initDevice(ma_device* device, const ma_device_config* config, ma_device_descriptor* playback, ma_device_descriptor* capture);
    static ma_result uninitDevice(ma_device* device);
    static ma_result startDevice(ma_device* device);
    static ma_result stopDevice(ma_device* device);
};

} // namespace haylen::audio
