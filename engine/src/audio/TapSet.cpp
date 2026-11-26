#include "audio/TapSet.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace haylen::audio {

TapSet::~TapSet() {
    for (const std::shared_ptr<Tap>& tap : taps) {
        tap->set = nullptr;
        tap->open.store(false, std::memory_order_release);
    }
}

std::shared_ptr<Tap> TapSet::open(std::uint32_t sampleRate, std::uint32_t channels, std::size_t capacityFrames) {
    if (capacityFrames == 0) {
        throw std::invalid_argument("A tap of the audio needs room for at least one frame.");
    }

    if (taps.size() == Mixer::kMaxTaps) {
        throw std::length_error("The audio has as many taps open as it holds. Close a tap before opening another one.");
    }

    auto tap = std::make_shared<Tap>(Tap::Passkey{}, *this, sampleRate, channels, capacityFrames);
    const auto free = std::ranges::find(slots, static_cast<Tap*>(nullptr), [](const std::atomic<Tap*>& slot) { return slot.load(std::memory_order_relaxed); });
    free->store(tap.get(), std::memory_order_seq_cst);
    active.fetch_add(1, std::memory_order_seq_cst);
    taps.push_back(tap);
    return tap;
}

// The slot empties before the count of blocks is read, so a block that the mix begins after that read finds the slot empty, and a tap that no block holds goes at once.
void TapSet::close(Tap& tap) {
    const auto found = std::ranges::find_if(taps, [&tap](const std::shared_ptr<Tap>& candidate) { return candidate.get() == &tap; });
    if (found == taps.end()) {
        return;
    }

    const auto slot = std::ranges::find(slots, &tap, [](const std::atomic<Tap*>& value) { return value.load(std::memory_order_relaxed); });
    slot->store(nullptr, std::memory_order_seq_cst);
    active.fetch_sub(1, std::memory_order_seq_cst);
    tap.set = nullptr;
    tap.open.store(false, std::memory_order_release);

    const std::uint64_t begun = blocks.load(std::memory_order_seq_cst);
    if (begun % 2 == 1) {
        retired.push_back({.tap = std::move(*found), .blocks = begun});
    }

    taps.erase(found);
}

void TapSet::collect() {
    const std::uint64_t now = blocks.load(std::memory_order_seq_cst);
    std::erase_if(retired, [now](const Retired& entry) { return entry.blocks != now; });

    for (std::size_t index = taps.size(); index > 0; --index) {
        if (taps[index - 1].use_count() == 1) {
            close(*taps[index - 1]);
        }
    }
}

void TapSet::deliver(const float* samples, std::uint64_t frames) noexcept {
    if (active.load(std::memory_order_relaxed) == 0) {
        return;
    }

    blocks.fetch_add(1, std::memory_order_seq_cst);
    for (const std::atomic<Tap*>& slot : slots) {
        if (Tap* tap = slot.load(std::memory_order_seq_cst)) {
            tap->write(samples, static_cast<std::size_t>(frames));
        }
    }

    blocks.fetch_add(1, std::memory_order_release);
}

} // namespace haylen::audio
