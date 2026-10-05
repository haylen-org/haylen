#include "audio/Device.hpp"

#include <thread>
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
    stop();
    disconnect();
    if (opening == nullptr) {
        return;
    }

    // The thread that still opens the device closes it once the system answers.
    const std::scoped_lock lock(opening->mutex);
    opening->abandoned = true;
    if (opening->finished) {
        close(*opening);
    }
}

void Device::start(ma_engine& source) {
    shared->engine.store(&source, std::memory_order_release);
    apply();
}

void Device::stop() noexcept {
    if (started) {
        ma_device_stop(&connection->device);
        started = false;
    }
    shared->engine.store(nullptr, std::memory_order_release);
    running = false;
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
    // An output that played warns when the system refuses it now, and an output that was unavailable already tries again quietly. An opening that waits for the system goes on.
    if (setup.device && opening == nullptr && (interrupted || connection == nullptr)) {
        const bool played = connection != nullptr;
        disconnect();
        connect(played);
    }
    interrupted = false;
    apply();
}

void Device::update(float deltaSeconds) {
    if (opening == nullptr) {
        return;
    }
    {
        const std::scoped_lock lock(opening->mutex);
        if (!opening->finished) {
            waited += deltaSeconds;
            if (waited >= kAnswerSeconds && !warnedWaiting) {
                warnedWaiting = true;
                core::Log::warning("The audio output does not answer, so the app runs without sound until the system opens it.");
            }
            return;
        }
    }
    take();
}

void Device::report(Mixer::DeviceEvent event) {
    const std::scoped_lock lock(shared->mutex);
    shared->events.push_back(event);
}

std::vector<Mixer::DeviceEvent> Device::takeEvents() {
    const std::scoped_lock lock(shared->mutex);
    return std::exchange(shared->events, {});
}

void Device::onData(ma_device* playback, void* output, const void*, ma_uint32 frames) {
    ma_engine* source = static_cast<Connection*>(playback->pUserData)->shared->engine.load(std::memory_order_acquire);
    if (source != nullptr) {
        ma_engine_read_pcm_frames(source, output, frames, nullptr);
    }
}

void Device::onNotification(const ma_device_notification* notification) noexcept {
    Shared& target = *static_cast<Connection*>(notification->pDevice->pUserData)->shared;
    std::optional<Mixer::DeviceEvent> event;
    switch (notification->type) {
    case ma_device_notification_type_interruption_began:
        event = Mixer::DeviceEvent::InterruptionBegan;
        break;
    case ma_device_notification_type_interruption_ended:
        event = Mixer::DeviceEvent::InterruptionEnded;
        break;
    case ma_device_notification_type_rerouted:
        event = Mixer::DeviceEvent::RouteChanged;
        break;
    default:
        return;
    }
    const std::scoped_lock lock(target.mutex);
    target.events.push_back(*event);
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

void Device::open(Connection& target, const Mixer::Setup& setup) {
    const std::optional<std::string_view> refusal = initialize(target, setup);
    const std::scoped_lock lock(target.mutex);
    target.finished = true;
    target.opened = !refusal;
    target.refusal = refusal;
    if (target.abandoned) {
        close(target);
    }
}

std::optional<std::string_view> Device::initialize(Connection& target, const Mixer::Setup& setup) {
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
    if (const ma_result result = ma_context_init(hosted ? &custom : nullptr, hosted ? 1U : 0U, &contextConfig, &target.context); result != MA_SUCCESS) {
        return hosted ? setup.backend->refusal : ma_result_description(result);
    }

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = setup.channels;
    config.sampleRate = setup.sampleRate;
    config.dataCallback = &Device::onData;
    config.notificationCallback = &Device::onNotification;
    config.pUserData = &target;
    config.noPreSilencedOutputBuffer = MA_TRUE;
    config.noClip = MA_TRUE;
    config.aaudio.usage = ma_aaudio_usage_game;
    config.aaudio.contentType = ma_aaudio_content_type_sonification;
    if (const ma_result result = ma_device_init(&target.context, &config, &target.device); result != MA_SUCCESS) {
        ma_context_uninit(&target.context);
        return ma_result_description(result);
    }
    return std::nullopt;
}

void Device::close(Connection& target) noexcept {
    if (!target.opened) {
        return;
    }
    ma_device_uninit(&target.device);
    ma_context_uninit(&target.context);
    target.opened = false;
}

void Device::connect(bool warn) {
    auto next = std::make_shared<Connection>();
    next->shared = shared;
    opening = next;
    waited = 0.0F;
    warnRefusal = warn;
    warnedWaiting = false;
#if defined(__EMSCRIPTEN__)
    open(*next, setup);
    take();
#else
    std::thread([next, options = setup] { open(*next, options); }).detach();
#endif
}

void Device::take() {
    const std::shared_ptr<Connection> done = std::exchange(opening, nullptr);
    if (done->refusal) {
        if (warnRefusal && !warnedWaiting) {
            core::Log::warning("The app runs without sound because the audio output is unavailable: {}.", *done->refusal);
        }
        return;
    }
    if (warnedWaiting) {
        core::Log::info("The audio output answered, and the app plays sound again.");
    }
    connection = done;
    apply();
}

void Device::disconnect() noexcept {
    if (connection == nullptr) {
        return;
    }
    if (started) {
        ma_device_stop(&connection->device);
        started = false;
    }
    close(*connection);
    connection.reset();
}

void Device::apply() {
    running = shared->engine.load(std::memory_order_acquire) != nullptr && !suspended && !interrupted;
    if (connection == nullptr || running == started) {
        return;
    }
    if (!running) {
        ma_device_stop(&connection->device);
        started = false;
        return;
    }
    if (const ma_result result = ma_device_start(&connection->device); result != MA_SUCCESS) {
        core::Log::warning("The audio output could not start ({}), so the app runs without sound until it resumes again.", ma_result_description(result));
        return;
    }
    started = true;
}

} // namespace haylen::audio
