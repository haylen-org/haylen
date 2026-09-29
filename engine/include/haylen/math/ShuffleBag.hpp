#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace haylen::math {

class Random;

// Deals item indices in random order without repeats until the bag runs out, then refills it, so every item keeps its share and streaks stay short. Item i goes into the bag counts[i] times.
class ShuffleBag final {
  public:
    // Throws std::invalid_argument when the bag would hold nothing.
    explicit ShuffleBag(std::span<const std::uint32_t> counts);

    [[nodiscard]] std::size_t next(Random& random) noexcept;
    void refill() noexcept;

    [[nodiscard]] std::size_t getRemaining() const noexcept {
        return remaining;
    }
    [[nodiscard]] std::size_t size() const noexcept {
        return items.size();
    }

  private:
    std::vector<std::size_t> items;
    std::size_t remaining = 0;
};

} // namespace haylen::math
