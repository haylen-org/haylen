#pragma once

#include <miniaudio.h>

namespace haylen::audio {

// A backend of its own that a host gives the audio device where the platform plays through something miniaudio does not know, such as the audio output of the page in browsers. The device then opens through this backend alone. The refusal explains in the log why the output is unavailable when the backend refuses to start.
struct OutputBackend {
    ma_result (*initContext)(ma_context* context, const ma_context_config* config, ma_backend_callbacks* callbacks) = nullptr;
    const char* refusal = "";
};

} // namespace haylen::audio
