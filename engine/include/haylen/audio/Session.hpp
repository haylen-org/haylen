#pragma once

#include <cstdint>

namespace haylen::audio {

// How the app shares sound with the system and other apps, which `app.json` sets in its `audio` object. The category is the audio session category of iOS and tvOS: ambient follows the silent switch and mixes with the audio of other apps, solo ambient follows the silent switch and silences other apps, and playback keeps playing with the silent switch on and silences other apps unless it mixes with them.
struct Session {
    enum class Category : std::uint8_t {
        Ambient,
        SoloAmbient,
        Playback,
    };

    Category category = Category::Ambient;

    // Lets the playback category play along with the audio of other apps. Ambient always mixes and solo ambient never does, so only playback accepts it.
    bool mixWithOthers = false;
};

} // namespace haylen::audio
