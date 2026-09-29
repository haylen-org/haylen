#include "plugins/AudioPlugin.hpp"

#include <memory>
#include <stdexcept>

#include "audio/AudioLua.hpp"
#include "audio/SoundData.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/audio/Sound.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/JsonValidator.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/plugins/AssetsPlugin.hpp"

namespace haylen::plugins {

void AudioPlugin::start(core::Engine& engine) {
    // clang-format off
    engine.getAssets().registerType({
        .name = "sound",
        .extensions = {".wav", ".ogg", ".mp3", ".flac"},
        .normalize = [](const core::Json& options) {
            core::JsonValidator::requireKnownKeys(options, {"stream"}, "sound options");
            return core::Json{{"stream", options.value("stream", false)}};
        },
        .decode = [](assets::Manager::Request& request) -> std::shared_ptr<void> {
            const audio::Sound sound = request.options.at("stream").get<bool>() ? audio::Sound::stream(std::move(request.bytes)) : audio::Sound::decode(request.bytes);
            return std::const_pointer_cast<audio::SoundData>(sound.getData());
        },
        .finalize = [](std::shared_ptr<void> decoded, const assets::Manager::Request&) { return decoded; },
    });
    engine.getPlugin<AssetsPlugin>().registerLuaPusher("sound", [](lua_State* L, const std::shared_ptr<void>& asset) {
        lua::Stack::push(L, audio::Sound(std::static_pointer_cast<const audio::SoundData>(asset)));
    });
    // clang-format on

    audio::Mixer& mixer = engine.getAudio();
    mixer.setProcessPaused(engine.isPaused());
    pauseConnection = engine.pausedChanged.connect([&mixer](bool paused) { mixer.setProcessPaused(paused); });
    deviceConnection = mixer.deviceEventReceived.connect([this, &engine](audio::Mixer::DeviceEvent deviceEvent) { handleDeviceEvent(engine, deviceEvent); });
    // iOS does not always report the end of an interruption, so the audio comes back whenever the app becomes active again, as Apple recommends.
    // clang-format off
    appStateConnection = engine.appStateChanged.connect([this, &engine](core::Engine::AppState value) {
        if (value == core::Engine::AppState::Active) {
            endInterruption(engine);
        }
    });
    // clang-format on
}

void AudioPlugin::stop(core::Engine& engine) {
    pauseConnection.disconnect();
    deviceConnection.disconnect();
    appStateConnection.disconnect();

    // A camera that Lua holds goes away with the Lua state.
    engine.getAudio().followCamera(nullptr);
}

void AudioPlugin::installLua(core::Engine&, lua_State* L) {
    audio::AudioLua::install(L);
}

// Platforms report interruptions that the audio device cannot see, such as the audio focus of Android, and they take the same path as the notifications of the device.
void AudioPlugin::event(core::Engine& engine, const platform::Event& event) {
    if (event.type == platform::Event::Type::InterruptionBegan) {
        handleDeviceEvent(engine, audio::Mixer::DeviceEvent::InterruptionBegan);
    } else if (event.type == platform::Event::Type::InterruptionEnded) {
        handleDeviceEvent(engine, audio::Mixer::DeviceEvent::InterruptionEnded);
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
    audio::Mixer& mixer = engine.getAudio();
    if (mixer.isInterrupted()) {
        return;
    }
    mixer.beginInterruption();
    engine.getEvents().emit(core::LifecycleEvent::kAudioInterrupted);
}

void AudioPlugin::endInterruption(core::Engine& engine) {
    audio::Mixer& mixer = engine.getAudio();
    if (!mixer.isInterrupted()) {
        return;
    }

    // The system may still hold the audio, and the next time the app becomes active tries again.
    try {
        mixer.endInterruption();
    } catch (const std::runtime_error& error) {
        core::Log::warning("{}", error.what());
        return;
    }
    engine.getEvents().emit(core::LifecycleEvent::kAudioResumed);
}

} // namespace haylen::plugins
