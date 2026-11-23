#include "plugins/AudioPlugin.hpp"

#include <memory>
#include <utility>

#include "audio/SoundData.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/audio/Sound.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/JsonValidator.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/platform/Event.hpp"

namespace haylen::plugins {

void AudioPlugin::start(core::Engine& engine) {
    // clang-format off
    engine.getAssets().registerType({
        .name = "sound",
        .extensions = {".wav", ".ogg", ".mp3", ".flac"},
        .normalize = [](const core::Json& options) {
            core::JsonValidator::requireKnownOptions(options, {"stream"}, "sound options");
            return core::Json{{"stream", options.value("stream", false)}};
        },
        .decode = [](assets::Manager::Request& request) -> std::shared_ptr<void> {
            const audio::Sound sound = request.reader ? audio::Sound::stream(std::move(request.reader)) : audio::Sound::decode(request.bytes);
            return std::const_pointer_cast<audio::SoundData>(sound.getData());
        },
        .finalize = [](std::shared_ptr<void> decoded, const assets::Manager::Request&) { return decoded; },
        .streams = [](const core::Json& options) { return options.at("stream").get<bool>(); },
        .reload = [](const std::shared_ptr<void>& asset, assets::Manager::Request& request) {
            const audio::Sound fresh = request.reader ? audio::Sound::stream(std::move(request.reader)) : audio::Sound::decode(request.bytes);
            std::static_pointer_cast<audio::SoundData>(asset)->replace(*fresh.getData());
        },
    });
    // clang-format on

    audio::Mixer& output = engine.getAudio();
    mixer = &output;
    output.setProcessPaused(engine.isPaused());
    pauseConnection = core::ScopedConnection(engine.pausedChanged.connect([&output](bool paused) { output.setProcessPaused(paused); }));
    deviceConnection = core::ScopedConnection(output.deviceEventReceived.connect([this, &engine](audio::Mixer::DeviceEvent deviceEvent) { handleDeviceEvent(engine, deviceEvent); }));
    // Because iOS does not always report the end of an interruption, the audio comes back whenever the app becomes active again, as Apple recommends, and an output that the system refused tries to open again then.
    // clang-format off
    appStateConnection = core::ScopedConnection(engine.appStateChanged.connect([this, &engine](core::Engine::AppState value) {
        if (value == core::Engine::AppState::Active) {
            endInterruption(engine);
        }
    }));
    // clang-format on
}

// Browsers keep the output locked until the user interacts with the page, so an app can show a sound hint until it plays.
void AudioPlugin::update(core::Engine& engine, float) {
    const audio::Mixer::OutputState current = engine.getAudio().getOutputState();
    if (std::exchange(outputState, current) == audio::Mixer::OutputState::Locked && current == audio::Mixer::OutputState::Running) {
        engine.getEvents().emit(core::LifecycleEvent::kAudioUnlocked);
    }
}

void AudioPlugin::stop(core::Engine& engine) {
    pauseConnection.disconnect();
    deviceConnection.disconnect();
    appStateConnection.disconnect();
    mixer = nullptr;

    // A camera that Lua holds goes away with the Lua state.
    engine.getAudio().followCamera(nullptr);
}

bool AudioPlugin::isAnimating() const {
    return mixer != nullptr && mixer->getOutputState() != outputState;
}

// Platforms report interruptions that the audio device cannot see, such as the audio focus of Android, and they take the same path as the notifications of the device. A reset of the audio services of the system, such as the media services of iOS, leaves the device silent, so the output opens again.
void AudioPlugin::event(core::Engine& engine, const platform::Event& event) {
    if (event.type == platform::Event::Type::InterruptionBegan) {
        handleDeviceEvent(engine, audio::Mixer::DeviceEvent::InterruptionBegan);
    } else if (event.type == platform::Event::Type::InterruptionEnded) {
        handleDeviceEvent(engine, audio::Mixer::DeviceEvent::InterruptionEnded);
    } else if (event.type == platform::Event::Type::AudioServicesReset) {
        engine.getAudio().restartOutput();
    }
}

void AudioPlugin::handleDeviceEvent(core::Engine& engine, audio::Mixer::DeviceEvent deviceEvent) {
    switch (deviceEvent) {
    case audio::Mixer::DeviceEvent::InterruptionBegan:
        beginInterruption(engine);
        break;
    case audio::Mixer::DeviceEvent::InterruptionEnded:
        if (engine.getAppState() == core::Engine::AppState::Active) {
            endInterruption(engine);
        }

        break;
    case audio::Mixer::DeviceEvent::RouteChanged:
        engine.getEvents().emit(core::LifecycleEvent::kAudioRouteChanged);
        break;
    }
}

void AudioPlugin::beginInterruption(core::Engine& engine) {
    audio::Mixer& output = engine.getAudio();
    if (output.isInterrupted()) {
        return;
    }

    output.beginInterruption();
    engine.getEvents().emit(core::LifecycleEvent::kAudioInterrupted);
}

void AudioPlugin::endInterruption(core::Engine& engine) {
    audio::Mixer& output = engine.getAudio();
    const bool interrupted = output.isInterrupted();
    output.endInterruption();

    if (interrupted) {
        engine.getEvents().emit(core::LifecycleEvent::kAudioResumed);
    }
}

} // namespace haylen::plugins
