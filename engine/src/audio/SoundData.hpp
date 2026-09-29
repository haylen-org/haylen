#pragma once

#include <cstdint>
#include <vector>

#include "haylen/debug/ObjectCounter.hpp"
#include "haylen/debug/TrackedObject.hpp"

namespace haylen::audio {

// The samples of a decoded sound, or the encoded file of a streamed one, with its format.
struct SoundData {
    static debug::ObjectCounter& counter;

    std::vector<float> samples;
    std::vector<std::uint8_t> encoded;
    std::uint32_t channels = 0;
    std::uint32_t sampleRate = 0;
    std::uint64_t frames = 0;
    bool streamed = false;
    debug::TrackedObject tracked{counter};
};

} // namespace haylen::audio
