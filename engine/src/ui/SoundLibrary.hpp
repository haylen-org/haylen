#pragma once

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "haylen/audio/Mixer.hpp"
#include "haylen/audio/Sound.hpp"
#include "haylen/ui/Theme.hpp"

namespace haylen::assets {
class Manager;
}

namespace haylen::ui {

// Loads the sounds that themes and styles give the interactions of the UI once, in the background, and plays them through the mixer on the frame thread. Each interaction plays one voice at a time: a play fades out the voice of the last play of its interaction, and a play sooner than the interval of its cue after that last play plays nothing, so a held key that moves the focus fast plays a steady row of ticks that never piles up.
class SoundLibrary final {
  public:
    // How long the voice of the last play of an interaction fades out when the next play cuts it, so the cut never clicks.
    static constexpr float kCutSeconds = 0.01F;

    SoundLibrary(assets::Manager& assetManager, audio::Mixer& audioMixer);

    SoundLibrary(const SoundLibrary&) = delete;
    SoundLibrary& operator=(const SoundLibrary&) = delete;

    // Starts loading a sound file of the package `content/` folder in the background, unless it is loading or loaded already.
    void load(std::string_view path);
    void load(const Theme& theme);

    // Plays the cue of an interaction at the app time `now` in seconds. A sound that is still loading plays nothing. Throws once the sound failed to load, and for a bus that the mixer does not have.
    void play(Theme::Sound role, const Theme::Cue& cue, double now);

    // Loads a sound read from a changed file again, and keeps playing the sound it had until the new one arrives.
    void reload(std::string_view path);

  private:
    struct File {
        audio::Sound sound;
        std::string error;
    };

    struct Channel {
        audio::Mixer::VoiceId voice = 0;
        std::optional<double> time;
    };

    // Returns the file of a path, which starts loading when it is new.
    File& find(std::string_view path);
    void request(const std::string& path);

    assets::Manager& assets;
    audio::Mixer& mixer;
    std::map<std::string, File, std::less<>> files;
    std::array<Channel, Theme::kSoundCount> channels{};

    // Loads that finish after the library is gone drop their result.
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
};

} // namespace haylen::ui
