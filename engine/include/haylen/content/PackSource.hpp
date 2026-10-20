#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>

#include "haylen/content/Error.hpp"

namespace haylen::content {

// Delivers packs, the content that a store installs apart from the app, such as an expansion that a store delivers on request. When the app loads an asset of a pack that the device lacks, the content manager asks each source in turn for the pack before it downloads from the content server, so the code that loads the asset never knows where the asset comes from. The native plugin of a store gives one through `ContentManager::addPackSource`, and Lua through `content.addPackSource`.
class PackSource {
  public:
    struct Progress {
        std::uint64_t receivedBytes = 0;
        std::uint64_t totalBytes = 0;
    };

    // Receives the folder that holds the shards of the delivered pack, nothing when the source does not deliver the pack, or the failure.
    using Delivery = std::function<void(std::optional<std::filesystem::path> folder, std::optional<Error> failure)>;

    virtual ~PackSource() = default;

    // Delivers the pack with the name and calls `done` once, on the frame thread, after any number of calls of `progress` there.
    virtual void acquire(const std::string& pack, std::function<void(const Progress&)> progress, Delivery done) = 0;
};

} // namespace haylen::content
