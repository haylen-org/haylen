#pragma once

#include <miniaudio.h>

namespace haylen::audio {

// A backend of its own that a host gives the audio device where the platform plays through something miniaudio does not know, such as the audio output of the page in browsers. The device then opens through this backend alone. The refusal explains in the log why the output is unavailable when the backend refuses to start.
struct OutputBackend {
    ma_result (*initContext)(ma_context* context, const ma_context_config* config, ma_backend_callbacks* callbacks) = nullptr;
    const char* refusal = "";

    // Whether the output of a device waits for the user before it plays, where the platform keeps sound locked, or null where it never does.
    bool (*isLocked)(ma_device* device) = nullptr;

    // The sentence that says why the output of a device stopped working after it opened, which the platform learns later, such as the page of a browser that refuses the audio context of a worker, or null while the output works. The function itself is null where the platform reports every failure while the device opens.
    const char* (*failure)(ma_device* device) = nullptr;
};

} // namespace haylen::audio
