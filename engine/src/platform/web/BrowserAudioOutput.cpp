#include "platform/web/BrowserAudioOutput.hpp"

#include <emscripten/emscripten.h>

// clang-format off
EM_JS(int, haylen_js_audio_supported, (), {
    return Module.haylen.audio.supported() ? 1 : 0;
});

EM_JS(int, haylen_js_audio_open, (void* device, int channels, int sampleRate, int blockFrames, int bufferedFrames), {
    return Module.haylen.audio.open(device, channels, sampleRate, blockFrames, bufferedFrames) ? 1 : 0;
});

EM_JS(void, haylen_js_audio_start, (void* device), {
    Module.haylen.audio.start(device);
});

EM_JS(void, haylen_js_audio_stop, (void* device), {
    Module.haylen.audio.stop(device);
});

EM_JS(void, haylen_js_audio_close, (void* device), {
    Module.haylen.audio.close(device);
});
// clang-format on

namespace haylen::audio {

ma_result BrowserAudioOutput::initContext(ma_context*, const ma_context_config*, ma_backend_callbacks* callbacks) {
    if (haylen_js_audio_supported() == 0) {
        return MA_FAILED_TO_INIT_BACKEND;
    }
    callbacks->onDeviceInit = &initDevice;
    callbacks->onDeviceUninit = &uninitDevice;
    callbacks->onDeviceStart = &startDevice;
    callbacks->onDeviceStop = &stopDevice;
    return MA_SUCCESS;
}

void BrowserAudioOutput::render(ma_device* device, float* samples, ma_uint32 frames) {
    ma_device_handle_backend_data_callback(device, samples, nullptr, frames);
}

// The page creates its AudioContext at the sample rate of the mixer, and the browser converts it to the rate of the hardware.
ma_result BrowserAudioOutput::initDevice(ma_device* device, const ma_device_config* config, ma_device_descriptor* playback, ma_device_descriptor*) {
    if (config->deviceType != ma_device_type_playback) {
        return MA_DEVICE_TYPE_NOT_SUPPORTED;
    }

    const ma_uint32 bufferedFrames = playback->sampleRate * kBufferedMilliseconds / 1000;
    if (haylen_js_audio_open(device, static_cast<int>(playback->channels), static_cast<int>(playback->sampleRate), static_cast<int>(kBlockFrames), static_cast<int>(bufferedFrames)) == 0) {
        return MA_FAILED_TO_OPEN_BACKEND_DEVICE;
    }

    playback->format = ma_format_f32;
    ma_channel_map_init_standard(ma_standard_channel_map_webaudio, playback->channelMap, MA_MAX_CHANNELS, playback->channels);
    playback->periodSizeInFrames = kBlockFrames;
    playback->periodCount = bufferedFrames / kBlockFrames;
    return MA_SUCCESS;
}

ma_result BrowserAudioOutput::uninitDevice(ma_device* device) {
    haylen_js_audio_close(device);
    return MA_SUCCESS;
}

ma_result BrowserAudioOutput::startDevice(ma_device* device) {
    haylen_js_audio_start(device);
    return MA_SUCCESS;
}

ma_result BrowserAudioOutput::stopDevice(ma_device* device) {
    haylen_js_audio_stop(device);
    return MA_SUCCESS;
}

} // namespace haylen::audio

// Entry point for the audio output of the page, called through Module.haylen.audio in platform/web/haylen-runtime.js.
extern "C" {

EMSCRIPTEN_KEEPALIVE void haylen_web_audio_render(ma_device* device, float* samples, int frames) {
    haylen::audio::BrowserAudioOutput::render(device, samples, static_cast<ma_uint32>(frames));
}
}
