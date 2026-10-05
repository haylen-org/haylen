#pragma once

#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/audio/Mixer.hpp"
#include "haylen/lua/EnumNames.hpp"

struct lua_State;

namespace haylen::lua {

template <> struct EnumNames<audio::Mixer::Spatialization::Model> {
    using Model = audio::Mixer::Spatialization::Model;

    static constexpr std::array<std::pair<std::string_view, Model>, 3> kNames{{{"linear", Model::Linear}, {"inverse", Model::Inverse}, {"exponential", Model::Exponential}}};

    static std::optional<Model> fromName(std::string_view name) {
        for (const auto& [candidate, model] : kNames) {
            if (candidate == name) {
                return model;
            }
        }
        return std::nullopt;
    }
    static std::string_view name(Model value) {
        for (const auto& [candidate, model] : kNames) {
            if (model == value) {
                return candidate;
            }
        }
        return kNames.front().first;
    }
};

} // namespace haylen::lua

namespace haylen::audio {

// Installs `haylen.audio`, which plays sounds and music through the mixer buses, and the `Sound` class.
class AudioLua final {
  public:
    static constexpr std::array<std::string_view, 12> kPlayFields{"bus", "volume", "pitch", "pitchVariation", "pan", "loop", "fadeIn", "startAt", "x", "y", "processMode", "effects"};

    static void install(lua_State* L);

    // Reads the options of `audio.play` at `index`, which may only hold the fields given, such as the options that apply to the voices of streams.
    [[nodiscard]] static Mixer::PlayOptions readPlayOptions(lua_State* L, int index, std::span<const std::string_view> fields);

  private:
    static constexpr std::array<std::string_view, 5> kMusicFields{"bus", "volume", "fade", "loop", "startAt"};
    static constexpr std::array<std::string_view, 4> kSoundFields{"stream", "format", "sampleRate", "channels"};
    static constexpr std::array<std::string_view, 7> kSpatializationFields{"model", "minDistance", "maxDistance", "rolloff", "panDistance", "doppler", "speedOfSound"};

    // The registry key of the camera the listener follows, which keeps the camera alive while it is followed.
    static constexpr const char* kCameraKey = "haylen.audio.camera";

    [[nodiscard]] static Mixer& getMixer(lua_State* L);
    [[nodiscard]] static std::vector<std::shared_ptr<Effect>> readEffects(lua_State* L, int index);
    static void pushEffects(lua_State* L, const std::vector<std::shared_ptr<Effect>>& effects);

    static int newSound(lua_State* L);

    // Reads samples of a type from the bytes of a Lua string, which may lie anywhere in memory.
    template <typename Sample> [[nodiscard]] static std::vector<Sample> readSamples(std::string_view bytes);

    static int play(lua_State* L);
    static int stop(lua_State* L);
    static int pause(lua_State* L);
    static int resume(lua_State* L);
    static int paused(lua_State* L);
    static int setVolume(lua_State* L);
    static int setPitch(lua_State* L);
    static int pitch(lua_State* L);
    static int seedVariation(lua_State* L);
    static int setPan(lua_State* L);
    static int setPosition(lua_State* L);
    static int processMode(lua_State* L);
    static int active(lua_State* L);
    static int cursor(lua_State* L);
    static int setCursor(lua_State* L);
    static int stopAll(lua_State* L);
    static int voiceCount(lua_State* L);
    static int pauseAll(lua_State* L);
    static int resumeAll(lua_State* L);
    static int interrupted(lua_State* L);
    static int addEffect(lua_State* L);
    static int removeEffect(lua_State* L);
    static int effects(lua_State* L);
    static int playMusic(lua_State* L);
    static int stopMusic(lua_State* L);
    static int music(lua_State* L);
    static int createBus(lua_State* L);
    static int setBusVolume(lua_State* L);
    static int busVolume(lua_State* L);
    static int setBusMuted(lua_State* L);
    static int busMuted(lua_State* L);
    static int setBusProcessMode(lua_State* L);
    static int busProcessMode(lua_State* L);
    static int addBusEffect(lua_State* L);
    static int removeBusEffect(lua_State* L);
    static int busEffects(lua_State* L);
    static int buses(lua_State* L);
    static int busStats(lua_State* L);
    static int setListener(lua_State* L);
    static int listener(lua_State* L);
    static int followCamera(lua_State* L);
    static int setSpatialization(lua_State* L);
    static int spatialization(lua_State* L);
    static int sampleRate(lua_State* L);
    static int channels(lua_State* L);
    static int hasDevice(lua_State* L);
    static int outputAvailable(lua_State* L);
    static int outputOpening(lua_State* L);

    static int soundChannels(lua_State* L);
    static int soundSampleRate(lua_State* L);
    static int soundFrameCount(lua_State* L);
    static int soundStreamed(lua_State* L);
    static int soundDuration(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::audio
