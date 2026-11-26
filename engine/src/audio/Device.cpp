#include "audio/Device.hpp"

#include <thread>
#include <utility>

#include "haylen/core/Log.hpp"
#include "haylen/port/Contract.hpp"

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

void Device::start(ma_engine& source, TapSet& taps) {
    shared->taps = &taps;
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

void Device::restart() {
    if (setup.device && opening == nullptr) {
        const bool played = connection != nullptr;
        disconnect();
        connect(played);
    }

    apply();
}

void Device::update(float deltaSeconds) {
    checkFailure();

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

// An output that failed after it opened leaves the app without sound, as one that the system refuses while it opens does, until the output opens again.
void Device::checkFailure() {
    if (connection == nullptr || !connection->hosted) {
        return;
    }

    const std::optional<std::string> reason = connection->hosted->getFailure();
    if (!reason) {
        return;
    }

    core::Log::warning("The audio output stopped working, so the app runs without sound until the system allows it again. {}", *reason);
    disconnect();
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
    const Shared& target = *static_cast<Connection*>(playback->pUserData)->shared;
    ma_engine* source = target.engine.load(std::memory_order_acquire);
    if (source != nullptr) {
        ma_engine_read_pcm_frames(source, output, frames, nullptr);
        target.taps->deliver(static_cast<const float*>(output), frames);
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
    std::optional<Refusal> refusal = initialize(target, setup);
    const std::scoped_lock lock(target.mutex);
    target.finished = true;
    target.opened = !refusal;
    target.refusal = std::move(refusal);
    if (target.abandoned) {
        close(target);
    }
}

std::optional<Device::Refusal> Device::initialize(Connection& target, const Mixer::Setup& setup) {
    // Creating the context sets the category of the iOS audio session and activates it, and other platforms ignore the session. Closing a context leaves the session active, because a closing that a restart or an abandoned opening runs late would otherwise silence the output that opened after it, and deactivating blocks the thread that closes.
    ma_context_config contextConfig = ma_context_config_init();
    contextConfig.coreaudio.noAudioSessionDeactivate = MA_TRUE;
    contextConfig.coreaudio.sessionCategory = toSessionCategory(setup.session.category);
    contextConfig.coreaudio.sessionCategoryOptions = setup.session.mixWithOthers ? static_cast<ma_uint32>(ma_ios_session_category_option_mix_with_others) : 0U;

    // The audio output that the host gives is the only one the context tries.
    const ma_backend custom = ma_backend_custom;
    const bool hosted = setup.output != nullptr;
    if (hosted) {
        target.hosted.emplace(setup.output, setup.latency);
        target.hosted->configure(contextConfig);
    }

    if (const ma_result result = ma_context_init(hosted ? &custom : nullptr, hosted ? 1U : 0U, &contextConfig, &target.context); result != MA_SUCCESS) {
        return refuse(result);
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
    config.performanceProfile = setup.latency == OutputSettings::Latency::PowerSaving ? ma_performance_profile_conservative : ma_performance_profile_low_latency;
    if (setup.latency == OutputSettings::Latency::Low) {
        config.periodSizeInMilliseconds = kLowLatencyPeriodMilliseconds;
    }

    config.aaudio.usage = ma_aaudio_usage_game;
    config.aaudio.contentType = ma_aaudio_content_type_sonification;
    if (const ma_result result = ma_device_init(&target.context, &config, &target.device); result != MA_SUCCESS) {
        ma_context_uninit(&target.context);
        if (hosted && !target.hosted->getRefusal().empty()) {
            return Refusal{.problem = Problem::Refused, .reason = target.hosted->getRefusal()};
        }

        return refuse(result);
    }

    return std::nullopt;
}

Device::Refusal Device::refuse(ma_result result) {
    const Problem problem = toProblem(result);
    return {.problem = problem, .reason = std::string(describe(problem))};
}

Device::Problem Device::toProblem(ma_result result) noexcept {
    switch (result) {
    case MA_NO_BACKEND:
    case MA_NO_DEVICE:
    case MA_DEVICE_TYPE_NOT_SUPPORTED:
    case MA_FAILED_TO_INIT_BACKEND:
    case MA_DOES_NOT_EXIST:
        return Problem::NoDevice;
    case MA_ACCESS_DENIED:
        return Problem::Denied;
    case MA_BUSY:
    case MA_ALREADY_IN_USE:
    case MA_SHARE_MODE_NOT_SUPPORTED:
        return Problem::Busy;
    case MA_FORMAT_NOT_SUPPORTED:
    case MA_INVALID_DEVICE_CONFIG:
        return Problem::UnsupportedFormat;
    case MA_OUT_OF_MEMORY:
        return Problem::OutOfMemory;
    default:
        return Problem::Failed;
    }
}

std::string_view Device::describe(Problem problem) noexcept {
    switch (problem) {
    case Problem::NoDevice:
        return "The system has no audio output to play on. Check that speakers or headphones are connected and turned on in the sound settings of the system.";
    case Problem::Denied:
        return "The system denied the app the audio output. Check the sound and privacy settings of the system.";
    case Problem::Busy:
        return "Another app holds the audio output for itself. Close the apps that keep the audio output to themselves.";
    case Problem::UnsupportedFormat:
        return "The audio output does not play the sample rate or the channels of the app. Check the format of the output in the sound settings of the system.";
    case Problem::OutOfMemory:
        return "The system ran out of memory to open the audio output. Close other apps and open the app again.";
    case Problem::Failed:
    case Problem::Refused:
        break;
    }

    return "The system could not open the audio output. Check the sound settings of the system and that its audio service runs.";
}

bool Device::isLocked() const noexcept {
    return connection != nullptr && connection->hosted && connection->hosted->isLocked();
}

float Device::getLatency() const noexcept {
    if (connection == nullptr) {
        return 0.0F;
    }

    const ma_device& device = connection->device;
    const ma_uint32 rate = device.playback.internalSampleRate;
    return rate > 0 ? static_cast<float>(device.playback.internalPeriodSizeInFrames * device.playback.internalPeriods) / static_cast<float>(rate) : 0.0F;
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

    // A platform without threads besides the one of the frames, such as a page, opens the device on that thread, where it never blocks.
    if (!port::Contract::hasThreads()) {
        open(*next, setup);
        take();
        return;
    }

    std::thread([next, options = setup] { open(*next, options); }).detach();
}

void Device::take() {
    const std::shared_ptr<Connection> done = std::exchange(opening, nullptr);
    if (done->refusal) {
        if (warnRefusal && !warnedWaiting) {
            core::Log::warning("The app could not open the audio output, so it runs without sound until the system allows it. {}", done->refusal->reason);
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
        core::Log::warning("The audio output could not start, so the app runs without sound until it resumes again. {}", describe(toProblem(result)));
        return;
    }

    started = true;
}

} // namespace haylen::audio
