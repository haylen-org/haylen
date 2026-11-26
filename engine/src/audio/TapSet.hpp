#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "haylen/audio/Mixer.hpp"
#include "haylen/audio/Tap.hpp"

namespace haylen::audio {

// The taps of the master bus of a mixer. The frame thread opens and closes them, and the thread that mixes hands every tap the frames it mixed through a fixed set of slots, so it never allocates, locks or waits. A tap that closes leaves its slot at once, and the set lets it go only once the mix left the block it may still be writing.
class TapSet final {
  public:
    TapSet() = default;

    // Closes every tap, once the output stopped mixing.
    ~TapSet();

    TapSet(const TapSet&) = delete;
    TapSet& operator=(const TapSet&) = delete;

    // Throws `std::invalid_argument` for a capacity of 0 and `std::length_error` while `Mixer::kMaxTaps` taps are open.
    [[nodiscard]] std::shared_ptr<Tap> open(std::uint32_t sampleRate, std::uint32_t channels, std::size_t capacityFrames);
    void close(Tap& tap);

    // Closes the taps that nothing but the set holds and lets go of the closed taps that the mix no longer writes. The mixer calls it once per frame.
    void collect();

    // Hands the frames that the mix just made to every open tap, on the thread that mixes.
    void deliver(const float* samples, std::uint64_t frames) noexcept;

    [[nodiscard]] std::size_t size() const noexcept {
        return taps.size();
    }

  private:
    // A closed tap that the mix may still be writing, with the count of blocks the mix had begun when it closed.
    struct Retired {
        std::shared_ptr<Tap> tap;
        std::uint64_t blocks = 0;
    };

    std::array<std::atomic<Tap*>, Mixer::kMaxTaps> slots{};
    std::atomic<std::size_t> active{0};

    // Grows by one when the mix begins a block and by one when it ends it, so an odd count means the mix writes a block now.
    std::atomic<std::uint64_t> blocks{0};
    std::vector<std::shared_ptr<Tap>> taps;
    std::vector<Retired> retired;
};

} // namespace haylen::audio
