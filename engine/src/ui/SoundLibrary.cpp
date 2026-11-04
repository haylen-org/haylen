#include "ui/SoundLibrary.hpp"

#include <stdexcept>
#include <utility>

#include "audio/SoundData.hpp"
#include "haylen/assets/Manager.hpp"

namespace haylen::ui {

SoundLibrary::SoundLibrary(assets::Manager& assetManager, audio::Mixer& audioMixer) : assets(assetManager), mixer(audioMixer) {}

void SoundLibrary::load(std::string_view path) {
    find(path);
}

void SoundLibrary::load(const Theme& theme) {
    for (std::size_t index = 0; index < Theme::kSoundCount; ++index) {
        if (const Theme::Cue* cue = theme.getSound(static_cast<Theme::Sound>(index))) {
            load(cue->sound);
        }
    }
}

void SoundLibrary::play(Theme::Sound role, const Theme::Cue& cue, double now) {
    const File& file = find(cue.sound);
    if (!file.error.empty()) {
        throw std::runtime_error("The UI sound \"" + cue.sound + "\" could not be loaded. " + file.error);
    }

    Channel& channel = channels[static_cast<std::size_t>(role)];
    if (!file.sound.isValid() || (channel.time && now - *channel.time < cue.interval)) {
        return;
    }

    mixer.stop(channel.voice, kCutSeconds);
    channel.voice = mixer.play(file.sound, {.bus = cue.bus, .volume = cue.volume});
    channel.time = now;
}

void SoundLibrary::reload(std::string_view path) {
    if (const auto found = files.find(path); found != files.end()) {
        request(found->first);
    }
}

SoundLibrary::File& SoundLibrary::find(std::string_view path) {
    if (const auto found = files.find(path); found != files.end()) {
        return found->second;
    }

    const auto added = files.emplace(std::string(path), File{}).first;
    request(added->first);
    return added->second;
}

// A load that fails keeps its error, which every play of the sound reports, until a reload brings the sound.
void SoundLibrary::request(const std::string& path) {
    // clang-format off
    assets.loadAsync("sound", path, [this, owner = std::weak_ptr<bool>(alive), path](std::shared_ptr<void> asset, std::string error) {
        if (owner.expired()) {
            return;
        }

        File& file = files.at(path);
        file.error = std::move(error);
        if (file.error.empty()) {
            file.sound = audio::Sound(std::static_pointer_cast<const audio::SoundData>(asset));
        }
    });
    // clang-format on
}

} // namespace haylen::ui
