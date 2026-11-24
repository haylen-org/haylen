#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "haylen/debug/ObjectCounter.hpp"
#include "haylen/debug/TrackedObject.hpp"
#include "haylen/io/PackageReader.hpp"

namespace haylen::audio {

// The audio of a sound, which a sound of a file that hot reload read again replaces as a whole, while every voice keeps the content it started with. Every sound that shares it, such as the handles that Lua holds of an asset, plays the new content from its next voice on.
class SoundData final {
  public:
    // The samples of a decoded sound, or the reader of the encoded file of a streamed one, with its format.
    struct Content {
        std::vector<float> samples{};
        std::shared_ptr<io::PackageReader> reader{};
        std::uint32_t channels = 0;
        std::uint32_t sampleRate = 0;
        std::uint64_t frames = 0;
    };

    static debug::ObjectCounter& counter;

    explicit SoundData(Content value);

    [[nodiscard]] std::shared_ptr<const Content> getContent() const;
    void replace(const SoundData& value);

  private:
    mutable std::mutex mutex;
    std::shared_ptr<const Content> content;
    debug::TrackedObject tracked{counter};
};

} // namespace haylen::audio
