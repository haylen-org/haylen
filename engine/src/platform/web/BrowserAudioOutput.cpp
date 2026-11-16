#include "platform/web/BrowserAudioOutput.hpp"
#include "platform/web/WebEntry.hpp"

#include <emscripten/emscripten.h>

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

EM_JS(int, haylen_js_audio_locked, (void* device), {
    return Module.haylen.mixer.locked(device) ? 1 : 0;
});

EM_JS(void, haylen_js_audio_close, (void* device), {
    Module.haylen.mixer.close(device);
});
// clang-format on

namespace haylen::audio {

const OutputBackend BrowserAudioOutput::kBackend{.initContext = &BrowserAudioOutput::initContext, .refusal = "browsers offer \"AudioWorklet\" only to pages served over \"https\" or from \"localhost\"", .isLocked = &BrowserAudioOutput::isLocked, .failure = &BrowserAudioOutput::getFailure};

std::unordered_map<ma_device*, std::string>& BrowserAudioOutput::failures = *new std::unordered_map<ma_device*, std::string>();

void BrowserAudioOutput::fail(ma_device* device, std::string reason) {
    failures.insert_or_assign(device, std::move(reason));
}

const char* BrowserAudioOutput::getFailure(ma_device* device) {
    const auto found = failures.find(device);
    return found != failures.end() ? found->second.c_str() : nullptr;
}

bool BrowserAudioOutput::isLocked(ma_device* device) {
    return haylen_js_audio_locked(device) != 0;
}

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

// The page creates its `AudioContext` at the sample rate of the mixer, and the browser converts it to the rate of the hardware.
ma_result BrowserAudioOutput::initDevice(ma_device* device, const ma_device_config* config, ma_device_descriptor* playback, ma_device_descriptor*) {
    if (config->deviceType != ma_device_type_playback) {
        return MA_DEVICE_TYPE_NOT_SUPPORTED;
    }

    const ma_uint32 bufferedFrames = playback->sampleRate * kBufferedMilliseconds / 1000;
    haylen_js_audio_open(device, static_cast<int>(playback->channels), static_cast<int>(playback->sampleRate), static_cast<int>(kBlockFrames), static_cast<int>(bufferedFrames));

    playback->format = ma_format_f32;
    ma_channel_map_init_standard(ma_standard_channel_map_webaudio, playback->channelMap, MA_MAX_CHANNELS, playback->channels);
    playback->periodSizeInFrames = kBlockFrames;
    playback->periodCount = bufferedFrames / kBlockFrames;
    return MA_SUCCESS;
}

ma_result BrowserAudioOutput::uninitDevice(ma_device* device) {
    failures.erase(device);
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

// Entry point for the mixer of the audio output, called through `Module.haylen.mixer` in `platform/web/haylen-runtime.js`.
extern "C" {

EMSCRIPTEN_KEEPALIVE void haylen_web_audio_render(ma_device* device, float* samples, int frames) {
    haylen::platform::WebEntry::guard("haylen_web_audio_render", [&] { haylen::audio::BrowserAudioOutput::render(device, samples, static_cast<ma_uint32>(frames)); });
}

// The page reports an output whose audio context or processor the browser refused, with the reason.
EMSCRIPTEN_KEEPALIVE void haylen_web_audio_failed(ma_device* device, const char* reason) {
    haylen::platform::WebEntry::guard("haylen_web_audio_failed", [&] { haylen::audio::BrowserAudioOutput::fail(device, reason); });
}
}
