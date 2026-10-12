#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "haylen/debug/ObjectCounter.hpp"
#include "haylen/debug/TrackedObject.hpp"
#include "haylen/io/PackageReader.hpp"

namespace haylen::audio {

// The samples of a decoded sound, or the reader of the encoded file of a streamed one, with its format.
struct SoundData {
    static debug::ObjectCounter& counter;

    std::vector<float> samples;
    std::shared_ptr<io::PackageReader> reader;
    std::uint32_t channels = 0;
    std::uint32_t sampleRate = 0;
    std::uint64_t frames = 0;
    debug::TrackedObject tracked{counter};
};

} // namespace haylen::audio
