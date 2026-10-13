#pragma once

#include "haylen/audio/Mixer.hpp"
#include "haylen/core/ScopedConnection.hpp"
#include "haylen/plugins/Plugin.hpp"

namespace haylen::plugins {

// Registers the `sound` asset type, which decodes `.wav`, `.ogg`, `.mp3` and `.flac` files and streams them instead when the `stream` option is set, and installs the `haylen.audio` module. It applies the engine pause to the mixer and handles audio interruptions from the audio device and from the platform: an interruption pauses the mixer and publishes `audioInterrupted`, its end resumes the mixer and publishes `audioResumed` once the app is active, a route change publishes `audioRouteChanged`, and an output that the platform unlocks publishes `audioUnlocked`. An output that the system refused tries to open again whenever the app becomes active.
class AudioPlugin final : public Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "audio";
    }
    void start(core::Engine& engine) override;
    void stop(core::Engine& engine) override;
    void installLua(core::Engine& engine, lua_State* L) override;
    void event(core::Engine& engine, const platform::Event& event) override;
    void update(core::Engine& engine, float deltaSeconds) override;

  private:
    void handleDeviceEvent(core::Engine& engine, audio::Mixer::DeviceEvent deviceEvent);
    void beginInterruption(core::Engine& engine);
    void endInterruption(core::Engine& engine);

    core::ScopedConnection pauseConnection;
    core::ScopedConnection appStateConnection;
    core::ScopedConnection deviceConnection;
    audio::Mixer::OutputState outputState = audio::Mixer::OutputState::Unavailable;
};

} // namespace haylen::plugins
