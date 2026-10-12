#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "content/crypto/Digest.hpp"

namespace haylen::content {

// The state of a local content store: the content manifest the app plays, the one before it that a recovery falls back to, the newest generation the app accepted from its channel, which keeps a server from rolling it back, and the journal that lists the records of the store. A store writes its state into two slots in turn, each with a sequence number and a checksum, so a write that a crash or a loss of power tears leaves the other slot whole.
class StoreState final {
  public:
    static constexpr std::uint16_t kVersion = 1;
    static constexpr std::size_t kMaximumSize = 4096;

    // The digest of the app, which keeps a store from serving another app.
    Digest application;

    // The build of the app that last wrote the state. A new build ships content of its own, so it starts from that content again.
    std::uint64_t appBuild = 0;

    // The active and the previous content manifest, zero for the content that ships with the app.
    Digest active;
    Digest previous;

    std::string channel;
    std::uint64_t acceptedGeneration = 0;
    Digest acceptedManifest;

    std::uint64_t journal = 0;
    std::uint64_t sequence = 0;

    [[nodiscard]] std::vector<std::uint8_t> write() const;

    // Reads a slot, and throws `CacheCorrupt` for one that is torn, damaged or of another version.
    [[nodiscard]] static StoreState parse(std::span<const std::uint8_t> bytes);

    friend bool operator==(const StoreState&, const StoreState&) = default;

  private:
    static constexpr std::array<std::uint8_t, 8> kMagic = {'H', 'S', 'T', 'A', 0x0D, 0x0A, 0x1A, 0x0A};
    static constexpr std::size_t kMaximumChannelLength = 64;
};

} // namespace haylen::content
