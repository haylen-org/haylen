#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/audio/Effect.hpp"
#include "haylen/audio/Session.hpp"
#include "haylen/audio/Sound.hpp"
#include "haylen/core/ProcessMode.hpp"
#include "haylen/core/Signal.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::core {
class JobSystem;
}

namespace haylen::graphics2d {
class Camera;
}

namespace haylen::platform {
class AudioStream;
}

namespace haylen::audio {

struct MixerState;
struct OutputBackend;

// Mixes sounds through named buses. The `master` bus feeds the output, and `music`, `sfx`, `ui` and `ambience` exist from the start. Every call runs on the frame thread, and calls with a voice that already finished do nothing. Volumes, pans, positions and spatialization settings must be finite, pitches finite and above 0 and fades from 0 to `kMaxFadeSeconds`, or the call throws `std::invalid_argument`.
class Mixer final {
  public:
    using VoiceId = std::uint64_t;

    static constexpr float kMaxFadeSeconds = 86400.0F;
    static constexpr std::uint8_t kDefaultPriority = 128;

    // Every stop, pause and steal fades over this long at least, so a waveform cut far from zero never clicks.
    static constexpr float kDeclickSeconds = 0.005F;

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

    // A mixer with a device plays in real time through the audio device of the platform. A mixer without one mixes only when `render` asks, or, when it keeps time, mixes the time of each update into silence, as a device that nobody hears would play it. Hosts give the device a backend of their own where the platform needs one, such as the audio output of the page in browsers.
    struct Setup {
        bool device = true;
        bool keepsTime = false;
        std::uint32_t sampleRate = 48000;
        std::uint32_t channels = 2;
        std::size_t maxVoices = 128;
        Session session{};
        const OutputBackend* backend = nullptr;
    };

    // A looping voice repeats the part of its sound between the loop start and the loop end in seconds, the whole sound by default, after it played from its start to the loop end once. A pitch variation picks the pitch of each play at random within pitch plus or minus the variation, so repeated sounds never sound exactly alike. The process mode decides whether the voice plays while the engine is paused, where `Inherit` takes the mode of its bus, and the effects process the voice in order before its bus. When every voice plays, a new one stops the voice of the lowest priority, the quietest among them and then the oldest.
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
        std::uint8_t priority = kDefaultPriority;
        float loopStart = 0.0F;
        std::optional<float> loopEnd;
    };

    // The voices that one sound may play at once and the seconds of app time between two plays of it. A play beyond the voices stops the oldest or the quietest voice of the sound, or starts nothing with `Steal::None`, and a play within the cooldown starts nothing, returning the id of a voice that already finished.
    struct Limit {
        enum class Steal : std::uint8_t {
            Oldest,
            Quietest,
            None,
        };

        std::size_t voices = 0;
        float cooldown = 0.0F;
        Steal steal = Steal::Oldest;

        [[nodiscard]] static std::optional<Steal> stealFromName(std::string_view name) noexcept;
        [[nodiscard]] static std::string_view stealName(Steal value) noexcept;

      private:
        static const std::array<std::pair<std::string_view, Steal>, 3> kStealNames;
    };

    // A looping track plays from its start to the loop end and repeats from the loop start, so one file holds an intro and a loop without a gap.
    struct MusicOptions {
        std::string bus = "music";
        float volume = 1.0F;
        float fade = 1.0F;
        bool loop = true;
        float startAt = 0.0F;
        float loopStart = 0.0F;
        std::optional<float> loopEnd;
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

    // What a bus plays right now, for debug statistics. The counts cover the voices that play through the bus itself, and `processing` tells whether its process mode runs in the current pause state.
    struct BusStats {
        std::string name;
        std::size_t voices = 0;
        std::size_t playing = 0;
        std::size_t paused = 0;
        bool processing = true;
    };

    // Throws `std::invalid_argument` for a setup without a sample rate, channels or voices, or that mixes with other apps outside the playback session, and `std::runtime_error` when the audio engine cannot start. The device opens on a thread of its own and plays from the first update after it opened. A device that the system refuses, or that does not answer for a few seconds, leaves the output unavailable, and the log writes one warning. The voices of streamed sounds decode on the I/O pool of the job system, which must outlive the mixer.
    Mixer(const Setup& setup, core::JobSystem& jobs);
    ~Mixer();

    Mixer(const Mixer&) = delete;
    Mixer& operator=(const Mixer&) = delete;

    // Starts a voice. When every voice is busy, the voice that is not music with the lowest priority stops to make room, the quietest of them and then the oldest, and the limit of the sound may steal one of its own voices or start nothing. Throws `std::invalid_argument` for an empty sound, an unknown bus, a pitch variation below 0 or not below the pitch, or a fade-in or start time that is not finite, and a call that throws stops no voice.
    VoiceId play(const Sound& sound, const PlayOptions& options = kDefaultPlayOptions);

    // Starts a voice that plays the audio stream as its samples arrive, resampled to the mixer, and silent where they have not arrived. It takes the stream over from the voice that played it before and never ends by itself. Throws `std::invalid_argument` for an empty stream and for options with a loop, a start time, a pitch other than 1 or a pitch variation, which a live stream cannot take.
    VoiceId play(const std::shared_ptr<platform::AudioStream>& stream, const PlayOptions& options = kDefaultPlayOptions);
    void stop(VoiceId voice, float fadeOutSeconds = 0.0F);

    // Limits the voices and the plays of a sound, which a limit of no voices and no cooldown removes. Throws `std::invalid_argument` for an empty sound or a cooldown that is negative or not finite.
    void setLimit(const Sound& sound, const Limit& limit);
    [[nodiscard]] std::optional<Limit> getLimit(const Sound& sound) const;

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

    // Returns `true` while the voice plays or is paused. A voice that was stopped is over at once, even while it fades out.
    [[nodiscard]] bool isActive(VoiceId voice) const;
    [[nodiscard]] float getCursor(VoiceId voice) const;

    // Moves the playback of a voice to a time in seconds, kept between 0 and the length of its sound, such as when a player drags the progress of a track. Throws `std::invalid_argument` for a time that is not finite and for a voice of a live stream, which plays its samples as they arrive.
    void setCursor(VoiceId voice, float seconds);
    void stopAll(float fadeOutSeconds = 0.0F);
    [[nodiscard]] std::size_t getVoiceCount() const noexcept;

    // Pauses every voice that exists now for the reason, and resumes the voices that the reason holds. Voices started later play.
    void pauseAll(PauseReason reason = PauseReason::App);
    void resumeAll(PauseReason reason = PauseReason::App);

    // Adds an effect at the end of the chain of a voice or a bus. Throws `std::invalid_argument` for an effect that already processes another bus or voice, or that another mixer used.
    void addEffect(VoiceId voice, std::shared_ptr<Effect> effect);
    void removeEffect(VoiceId voice, const Effect& effect);
    [[nodiscard]] std::vector<std::shared_ptr<Effect>> getEffects(VoiceId voice) const;
    void addBusEffect(std::string_view bus, std::shared_ptr<Effect> effect);
    void removeBusEffect(std::string_view bus, const Effect& effect);
    [[nodiscard]] std::vector<std::shared_ptr<Effect>> getBusEffects(std::string_view bus) const;

    // Plays one music track at a time from its start time, crossfading from the previous track over the fade time, and returns the voice of the track, which pauses, stops, moves its cursor and takes volume and effects like any voice. Asking for the track that is already playing keeps its voice, paused or not, and only changes its volume. A track that cannot play throws and leaves the current track playing.
    VoiceId playMusic(const Sound& sound, const MusicOptions& options = kDefaultMusicOptions);
    void stopMusic(float fadeOutSeconds = 1.0F);

    // Returns the track that plays or is paused, or an empty sound once its voice stops or finishes.
    [[nodiscard]] Sound getMusic() const;

    void createBus(const std::string& name, std::string_view parent = "master");
    void setBusVolume(std::string_view bus, float volume, float fadeSeconds = 0.0F);
    [[nodiscard]] float getBusVolume(std::string_view bus) const;
    void setBusMuted(std::string_view bus, bool muted);
    [[nodiscard]] bool isBusMuted(std::string_view bus) const;

    // Silences the output after every bus, which the engine does while the app state or a cover asks for it, without touching the mutes of the buses that the app sets.
    void setOutputMuted(bool value);
    [[nodiscard]] bool isOutputMuted() const noexcept;

    // Sets the process mode that the voices of a bus inherit. The mode `Inherit` takes the mode of the parent bus, and `master` resolves it to `Pausable`. The buses `music` and `ui` start as `Always`, `sfx` and `ambience` as `Pausable`.
    void setBusProcessMode(std::string_view bus, core::ProcessMode mode);
    [[nodiscard]] core::ProcessMode getBusProcessMode(std::string_view bus) const;
    [[nodiscard]] std::vector<std::string> getBuses() const;
    [[nodiscard]] std::vector<BusStats> getBusStats() const;

    // Applies the pause of the engine to every voice by its process mode. The engine calls it whenever its pause changes.
    void setProcessPaused(bool value);
    [[nodiscard]] bool isProcessPaused() const noexcept;

    void setListener(math::Vec2 position);
    [[nodiscard]] math::Vec2 getListener() const noexcept;

    // Moves the listener to the camera position and offset on every update, without its shake, until null stops it. The camera must outlive the following.
    void followCamera(const graphics2d::Camera* camera) noexcept;

    // Throws `std::invalid_argument` for distances outside 0 <= minimum < maximum, a minimum of 0 with the inverse or exponential model, a negative rolloff or Doppler factor, or a pan distance or speed of sound that is not positive.
    void setSpatialization(const Spatialization& value);
    [[nodiscard]] const Spatialization& getSpatialization() const noexcept;

    // Stops the output while the app is in the background.
    void suspend();
    void resume();

    // Queues an event from any thread for the next update, which emits it through `deviceEventReceived`. The device of the mixer reports its own events, and platform code reports what the device cannot see.
    void reportDeviceEvent(DeviceEvent event);

    // Stops the output and pauses every voice for the interruption. Ending it resumes the voices and opens the device again, which reactivates the iOS audio session and restarts the output unless the app is in the background. When the system refuses the audio, the interruption ends all the same and the output stays unavailable, with one warning in the log. Ending an interruption while the output is unavailable, even without one in progress, tries to open the device again.
    void beginInterruption();
    void endInterruption();
    [[nodiscard]] bool isInterrupted() const noexcept;

    // Opens the audio device again, such as after the system reset its audio services, which leaves every output it had open silent. The voices go on and play from the first frame after the device opened.
    void restartOutput();

    // Emits the device events that arrived, mixes and discards the seconds since the last update while the output of a mixer with a device is unavailable, releases finished voices, refreshes positional voices, measuring their velocities over those seconds, and posts the fills that the voices of streamed sounds need to the I/O pool. The engine calls it once per frame.
    void update(float deltaSeconds);

    [[nodiscard]] std::uint32_t getSampleRate() const noexcept;
    [[nodiscard]] std::uint32_t getChannels() const noexcept;
    [[nodiscard]] bool hasDevice() const noexcept;

    // Tells whether the mix reaches an audio device. It is `false` for a mixer without a device, while the system opens the device and while it refuses the device, when voices, music and fades go on in real time without sound.
    [[nodiscard]] bool isOutputAvailable() const noexcept;

    // Tells whether the system is still opening the audio device, which happens on a thread of its own when the mixer starts and when an interruption ends, so a device that answers slowly or never never stalls the frames.
    [[nodiscard]] bool isOutputOpening() const noexcept;

    // Mixes the next frames into interleaved samples, or silence while the output is stopped. Only a mixer without a device that keeps no time renders this way, which tests and offline tools use.
    void render(std::span<float> samples);

    core::Signal<DeviceEvent> deviceEventReceived;

  private:
    static const PlayOptions kDefaultPlayOptions;
    static const MusicOptions kDefaultMusicOptions;

    static void requireFinite(float value, const char* name);
    static void requireFade(float seconds);
    static void requirePitch(float value);
    static void requirePosition(math::Vec2 position);
    static void requirePlayOptions(const PlayOptions& options);
    static void requireLoop(const PlayOptions& options, float duration);
    [[nodiscard]] static std::uint64_t toFrame(float seconds, std::uint32_t sampleRate) noexcept;

    std::unique_ptr<MixerState> state;
};

} // namespace haylen::audio
