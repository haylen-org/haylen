#pragma once

#include <miniaudio.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "audio/Device.hpp"
#include "audio/EffectChain.hpp"
#include "haylen/audio/Mixer.hpp"
#include "haylen/audio/Sound.hpp"
#include "haylen/core/ProcessMode.hpp"
#include "haylen/math/Random.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics2d {
class Camera;
}

namespace haylen::audio {

// The miniaudio engine of a mixer with its device, buses and playing voices.
struct MixerState {
    struct Bus {
        std::string name;
        Bus* parent = nullptr;
        ma_sound_group group{};
        std::unique_ptr<EffectChain> effects;
        core::ProcessMode processMode = core::ProcessMode::Inherit;
        float volume = 1.0F;
        bool muted = false;
    };

    // One playing sound with its own data source, so every voice keeps an independent cursor over shared sound data. A voice plays only while nothing holds it: its own pause, a pause of every voice for a reason, or its process mode during the engine pause.
    struct Voice {
        static constexpr std::uint8_t kVoiceHold = 1U << 0U;
        static constexpr std::uint8_t kAppHold = 1U << 1U;
        static constexpr std::uint8_t kInterruptionHold = 1U << 2U;

        Mixer::VoiceId id = 0;
        Sound sound;
        Bus* bus = nullptr;
        ma_audio_buffer buffer{};
        ma_decoder decoder{};
        ma_sound handle{};
        bool decoderSource = false;
        bool sourceReady = false;
        bool handleReady = false;
        std::unique_ptr<EffectChain> effects;
        core::ProcessMode processMode = core::ProcessMode::Inherit;
        float volume = 1.0F;
        float pan = 0.0F;
        float pitch = 1.0F;
        std::optional<math::Vec2> position;
        math::Vec2 lastPosition{};
        math::Vec2 velocity{};
        std::uint8_t holds = 0;
        bool processHeld = false;
        bool started = false;
        bool stopped = false;
        bool music = false;

        Voice() = default;
        ~Voice();

        Voice(const Voice&) = delete;
        Voice& operator=(const Voice&) = delete;

        [[nodiscard]] bool isHeld() const noexcept {
            return holds != 0 || processHeld;
        }
        void setHold(std::uint8_t hold, bool value) noexcept {
            holds = static_cast<std::uint8_t>(value ? holds | hold : holds & ~hold);
        }
    };

    // The effects of a finished voice, which keep sounding into its bus until the end frame of the engine clock.
    struct Tail {
        std::unique_ptr<EffectChain> effects;
        ma_uint64 endFrame = 0;
    };

    static constexpr ma_uint64 kSilentBlockFrames = 1024;

    explicit MixerState(const Mixer::Setup& setup) : output(setup) {}
    ~MixerState();

    MixerState(const MixerState&) = delete;
    MixerState& operator=(const MixerState&) = delete;

    [[nodiscard]] static ma_uint64 toMilliseconds(float seconds);
    [[nodiscard]] static std::uint8_t toHold(Mixer::PauseReason reason) noexcept;

    [[nodiscard]] Bus& getBus(std::string_view name) const;
    [[nodiscard]] Voice* findVoice(Mixer::VoiceId id) const noexcept;
    void addBus(const std::string& name, Bus* parent);

    // Resolves the process mode of a voice through its bus and the buses above it, where Inherit at master means Pausable.
    [[nodiscard]] core::ProcessMode resolveMode(const Voice& voice) const noexcept;
    [[nodiscard]] core::ProcessMode resolveMode(const Bus& bus) const noexcept;

    // Starts or stops a voice to match what holds it.
    void refresh(Voice& voice);
    void refreshAll();

    // Applies the voice volume, pan and pitch and, for positional voices, the distance, direction and motion relative to the listener.
    void apply(Voice& voice) const;

    // Hands the effects of a voice that is about to go over to the tails when they ring on.
    void retire(Voice& voice);

    [[nodiscard]] EffectChain& getEffects(Voice& voice);

    // Mixes and discards the frames of the elapsed seconds while the output runs without a device to play it, so voices, fades and effect tails move on in real time without sound.
    void advance(float deltaSeconds);

    // Releases the voices that finished and the tails that rang out.
    void releaseFinished();

    // Follows the camera, measures the velocities of the listener and of positional voices over the elapsed seconds and applies their spatialization.
    void spatialize(float deltaSeconds);

    void makeRoom();

    // The output goes first, so the engine can mix into it, and is destroyed last, after the destructor stops the audio thread with the engine.
    Device output;
    ma_engine engine{};
    bool engineReady = false;
    std::size_t maxVoices = 0;
    std::map<std::string, std::unique_ptr<Bus>, std::less<>> buses;
    std::vector<std::string> busOrder;
    std::vector<std::unique_ptr<Voice>> voices;
    std::vector<Tail> tails;
    Mixer::VoiceId nextVoice = 1;
    Mixer::VoiceId musicVoice = 0;
    math::Vec2 listener{};
    math::Vec2 lastListener{};
    math::Vec2 listenerVelocity{};
    const graphics2d::Camera* camera = nullptr;
    Mixer::Spatialization spatialization;
    math::Random random;
    bool processPaused = false;
    std::vector<float> silentBlock;
    double silentFrames = 0.0;
};

} // namespace haylen::audio
