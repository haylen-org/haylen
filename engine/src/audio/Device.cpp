#include "audio/Device.hpp"

#include <utility>

#include "audio/OutputBackend.hpp"
#include "haylen/core/Log.hpp"

namespace haylen::audio {

Device::Device(const Mixer::Setup& value) : setup(value) {
    if (setup.device) {
        connect(true);
    }
}

Device::~Device() {
    close();
}

void Device::start(ma_engine& source) {
    engine = &source;
    apply();
}

void Device::suspend() {
    suspended = true;
    apply();
}

void Device::resume() {
    suspended = false;
    apply();
}

void Device::interrupt() {
    interrupted = true;
    apply();
}

void Device::endInterruption() {
    // An output that played warns when the system refuses it now, and an output that was unavailable already tries again quietly.
    if (setup.device && (interrupted || !opened)) {
        const bool played = opened;
        close();
        connect(played);
    }
    interrupted = false;
    apply();
}

void Device::report(Mixer::DeviceEvent event) {
    const std::lock_guard lock(eventMutex);
    events.push_back(event);
}

std::vector<Mixer::DeviceEvent> Device::takeEvents() {
    const std::lock_guard lock(eventMutex);
    return std::exchange(events, {});
}

void Device::onData(ma_device* playback, void* output, const void*, ma_uint32 frames) {
    ma_engine_read_pcm_frames(static_cast<Device*>(playback->pUserData)->engine, output, frames, nullptr);
}

void Device::onNotification(const ma_device_notification* notification) noexcept {
    auto* owner = static_cast<Device*>(notification->pDevice->pUserData);
    switch (notification->type) {
    case ma_device_notification_type_interruption_began:
        owner->report(Mixer::DeviceEvent::InterruptionBegan);
        break;
    case ma_device_notification_type_interruption_ended:
        owner->report(Mixer::DeviceEvent::InterruptionEnded);
        break;
    case ma_device_notification_type_rerouted:
        owner->report(Mixer::DeviceEvent::RouteChanged);
        break;
    default:
        break;
    }
}

ma_ios_session_category Device::toSessionCategory(Session::Category category) noexcept {
    switch (category) {
    case Session::Category::Ambient:
        return ma_ios_session_category_ambient;
    case Session::Category::SoloAmbient:
        return ma_ios_session_category_solo_ambient;
    case Session::Category::Playback:
        return ma_ios_session_category_playback;
    }
    return ma_ios_session_category_ambient;
}

std::optional<std::string_view> Device::open() {
    // Creating the context sets the category of the iOS audio session and activates it, and other platforms ignore the session.
    ma_context_config contextConfig = ma_context_config_init();
    contextConfig.coreaudio.sessionCategory = toSessionCategory(setup.session.category);
    contextConfig.coreaudio.sessionCategoryOptions = setup.session.mixWithOthers ? static_cast<ma_uint32>(ma_ios_session_category_option_mix_with_others) : 0U;

    // A backend that the host gives is the only one the context tries.
    const ma_backend custom = ma_backend_custom;
    const bool hosted = setup.backend != nullptr;
    if (hosted) {
        contextConfig.custom.onContextInit = setup.backend->initContext;
    }
    if (const ma_result result = ma_context_init(hosted ? &custom : nullptr, hosted ? 1U : 0U, &contextConfig, &context); result != MA_SUCCESS) {
        return hosted ? setup.backend->refusal : ma_result_description(result);
    }

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = setup.channels;
    config.sampleRate = setup.sampleRate;
    config.dataCallback = &Device::onData;
    config.notificationCallback = &Device::onNotification;
    config.pUserData = this;
    config.noPreSilencedOutputBuffer = MA_TRUE;
    config.noClip = MA_TRUE;
    config.aaudio.usage = ma_aaudio_usage_game;
    config.aaudio.contentType = ma_aaudio_content_type_sonification;
    if (const ma_result result = ma_device_init(&context, &config, &device); result != MA_SUCCESS) {
        ma_context_uninit(&context);
        return ma_result_description(result);
    }
    opened = true;
    return std::nullopt;
}

void Device::connect(bool warn) {
    const std::optional<std::string_view> refusal = open();
    if (refusal && warn) {
        core::Log::warning("The app runs without sound because the audio output is unavailable: {}.", *refusal);
    }
}

void Device::close() noexcept {
    if (!opened) {
        return;
    }
    ma_device_uninit(&device);
    ma_context_uninit(&context);
    opened = false;
    started = false;
}

void Device::apply() {
    running = engine != nullptr && !suspended && !interrupted;
    if (!opened || running == started) {
        return;
    }
    if (!running) {
        ma_device_stop(&device);
        started = false;
        return;
    }
    if (const ma_result result = ma_device_start(&device); result != MA_SUCCESS) {
        core::Log::warning("The audio output could not start ({}), so the app runs without sound until it resumes again.", ma_result_description(result));
        return;
    }
    started = true;
}

} // namespace haylen::audio
