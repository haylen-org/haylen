#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/audio/Effect.hpp"
#include "haylen/audio/Session.hpp"
#include "haylen/audio/Sound.hpp"
#include "haylen/core/ProcessMode.hpp"
#include "haylen/core/Signal.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics2d {
class Camera;
}

namespace haylen::audio {

struct MixerState;

// Mixes sounds through named buses. The master bus feeds the output, and music, sfx, ui and ambience exist from the start. Every call runs on the frame thread, and calls with a voice that already finished do nothing.
class Mixer final {
  public:
    using VoiceId = std::uint64_t;

    // Why every voice pauses at once. Each reason holds voices on its own, so ending one pause never resumes a voice that another reason or the app still holds.
    enum class PauseReason : std::uint8_t {
        App,
        Interruption,
    };

    // What the audio device or the platform reports. Calls, alarms and Siri interrupt the audio on iOS, the audio focus does on Android, and the route changes when the output moves, such as when headphones are unplugged.
    enum class DeviceEvent : std::uint8_t {
        InterruptionBegan,
        InterruptionEnded,
        RouteChanged,
    };

    struct Setup {
        bool device = true;
        std::uint32_t sampleRate = 48000;
        std::uint32_t channels = 2;
        std::size_t maxVoices = 128;
        Session session{};
    };

    // A pitch variation picks the pitch of each play at random within pitch plus or minus the variation, so repeated sounds never sound exactly alike. The process mode decides whether the voice plays while the engine is paused, where Inherit takes the mode of its bus, and the effects process the voice in order before its bus.
    struct PlayOptions {
        std::string bus = "sfx";
        float volume = 1.0F;
        float pitch = 1.0F;
        float pitchVariation = 0.0F;
        float pan = 0.0F;
        bool loop = false;
        float fadeIn = 0.0F;
        float startAt = 0.0F;
        std::optional<math::Vec2> position;
        core::ProcessMode processMode = core::ProcessMode::Inherit;
        std::vector<std::shared_ptr<Effect>> effects;
    };

    struct MusicOptions {
        std::string bus = "music";
        float volume = 1.0F;
        float fade = 1.0F;
        bool loop = true;
    };

    // How positional voices fade, pan and shift in pitch with their place relative to the listener. The fade models are those of OpenAL between the minimum and the maximum distance: linear reaches silence at the maximum distance with a rolloff of 1, inverse and exponential keep fading more slowly and stop at the maximum distance. A voice pans by its horizontal offset over the pan distance, and a Doppler factor above 0 shifts the pitch of voices that move toward or away from the listener, with the speed of sound in world units per second.
    struct Spatialization {
        enum class Model : std::uint8_t {
            Linear,
            Inverse,
            Exponential,
        };

        Model model = Model::Linear;
        float minDistance = 100.0F;
        float maxDistance = 1500.0F;
        float rolloff = 1.0F;
        float panDistance = 1500.0F;
        float doppler = 0.0F;
        float speedOfSound = 3430.0F;

        static constexpr float kMinDopplerPitch = 0.25F;
        static constexpr float kMaxDopplerPitch = 4.0F;

        [[nodiscard]] float getGain(float distance) const noexcept;
        [[nodiscard]] float getPan(math::Vec2 offset) const noexcept;

        // Returns the pitch ratio of a voice at the offset from the listener, which stays within two octaves up or down.
        [[nodiscard]] float getDopplerPitch(math::Vec2 offset, math::Vec2 listenerVelocity, math::Vec2 voiceVelocity) const noexcept;
    };

    // What a bus plays right now, for debug statistics. The counts cover the voices that play through the bus itself, and processing tells whether its process mode runs in the current pause state.
    struct BusStats {
        std::string name;
        std::size_t voices = 0;
        std::size_t playing = 0;
        std::size_t paused = 0;
        bool processing = true;
    };

    // Throws std::invalid_argument for a setup without a sample rate, channels or voices, or that mixes with other apps outside the playback session, and std::runtime_error when the audio device cannot open.
    explicit Mixer(const Setup& setup);
    ~Mixer();

    Mixer(const Mixer&) = delete;
    Mixer& operator=(const Mixer&) = delete;

    // Starts a voice. When every voice is busy, the oldest voice that is not music stops to make room.
    VoiceId play(const Sound& sound, const PlayOptions& options = kDefaultPlayOptions);
    void stop(VoiceId voice, float fadeOutSeconds = 0.0F);

    // Pauses or resumes a voice for the app. A voice that the app paused stays paused when the engine pause or a pause of every voice ends.
    void setPaused(VoiceId voice, bool paused);
    [[nodiscard]] bool isPaused(VoiceId voice) const;
    void setVolume(VoiceId voice, float volume);
    void setPitch(VoiceId voice, float pitch);
    [[nodiscard]] float getPitch(VoiceId voice) const;

    // Seeds the generator behind pitch variation, which makes it repeatable.
    void seedVariation(std::uint64_t seed) noexcept;
    void setPan(VoiceId voice, float pan);
    void setPosition(VoiceId voice, math::Vec2 position);
    [[nodiscard]] core::ProcessMode getProcessMode(VoiceId voice) const;
    [[nodiscard]] bool isActive(VoiceId voice) const;
    [[nodiscard]] float getCursor(VoiceId voice) const;
    void stopAll(float fadeOutSeconds = 0.0F);
    [[nodiscard]] std::size_t getVoiceCount() const noexcept;

    // Pauses every voice that exists now for the reason, and resumes the voices that the reason holds. Voices started later play.
    void pauseAll(PauseReason reason = PauseReason::App);
    void resumeAll(PauseReason reason = PauseReason::App);

    // Adds an effect at the end of the chain of a voice or a bus. Throws std::invalid_argument for an effect that already processes another bus or voice, or that another mixer used.
    void addEffect(VoiceId voice, std::shared_ptr<Effect> effect);
    void removeEffect(VoiceId voice, const Effect& effect);
    [[nodiscard]] std::vector<std::shared_ptr<Effect>> getEffects(VoiceId voice) const;
    void addBusEffect(std::string_view bus, std::shared_ptr<Effect> effect);
    void removeBusEffect(std::string_view bus, const Effect& effect);
    [[nodiscard]] std::vector<std::shared_ptr<Effect>> getBusEffects(std::string_view bus) const;

    // Plays one music track at a time, crossfading from the previous track over the fade time.
    void playMusic(const Sound& sound, const MusicOptions& options = kDefaultMusicOptions);
    void stopMusic(float fadeOutSeconds = 1.0F);
    [[nodiscard]] Sound getMusic() const;

    void createBus(const std::string& name, std::string_view parent = "master");
    void setBusVolume(std::string_view bus, float volume, float fadeSeconds = 0.0F);
    [[nodiscard]] float getBusVolume(std::string_view bus) const;
    void setBusMuted(std::string_view bus, bool muted);
    [[nodiscard]] bool isBusMuted(std::string_view bus) const;

    // Sets the process mode that the voices of a bus inherit. Inherit takes the mode of the parent bus, and master resolves it to Pausable. Music and ui start as Always, sfx and ambience as Pausable.
    void setBusProcessMode(std::string_view bus, core::ProcessMode mode);
    [[nodiscard]] core::ProcessMode getBusProcessMode(std::string_view bus) const;
    [[nodiscard]] std::vector<std::string> getBuses() const;
    [[nodiscard]] std::vector<BusStats> getBusStats() const;

    // Applies the pause of the engine to every voice by its process mode. The engine calls it whenever its pause changes.
    void setProcessPaused(bool value);
    [[nodiscard]] bool isProcessPaused() const noexcept;

    void setListener(math::Vec2 position) noexcept;
    [[nodiscard]] math::Vec2 getListener() const noexcept;

    // Moves the listener to the camera position and offset on every update, without its shake, until null stops it. The camera must outlive the following.
    void followCamera(const graphics2d::Camera* camera) noexcept;

    // Throws std::invalid_argument for distances outside 0 <= minimum < maximum, a minimum of 0 with the inverse or exponential model, a negative rolloff or Doppler factor, or a pan distance or speed of sound that is not positive.
    void setSpatialization(const Spatialization& value);
    [[nodiscard]] const Spatialization& getSpatialization() const noexcept;

    // Stops the output while the app is in the background.
    void suspend();
    void resume();

    // Queues an event from any thread for the next update, which emits it through deviceEventReceived. The device of the mixer reports its own events, and platform code reports what the device cannot see.
    void reportDeviceEvent(DeviceEvent event);

    // Stops the output and pauses every voice for the interruption. Ending it opens the device again, which reactivates the iOS audio session, restarts the output unless the app is in the background and resumes the voices. Ending throws std::runtime_error when the system refuses the audio, and the mixer stays interrupted.
    void beginInterruption();
    void endInterruption();
    [[nodiscard]] bool isInterrupted() const noexcept;

    // Emits the device events that arrived, releases finished voices and refreshes positional voices, measuring their velocities over the seconds since the last update. The engine calls it once per frame.
    void update(float deltaSeconds);

    [[nodiscard]] std::uint32_t getSampleRate() const noexcept;
    [[nodiscard]] std::uint32_t getChannels() const noexcept;
    [[nodiscard]] bool hasDevice() const noexcept;

    // Mixes the next frames into interleaved samples, or silence while the output is stopped. Only a mixer without a device renders this way, which tests and offline tools use.
    void render(std::span<float> samples);

    core::Signal<DeviceEvent> deviceEventReceived;

  private:
    static const PlayOptions kDefaultPlayOptions;
    static const MusicOptions kDefaultMusicOptions;

    std::unique_ptr<MixerState> state;
};

} // namespace haylen::audio
