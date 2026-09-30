#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace haylen::math {

class Random;

// Picks indices in proportion to fixed weights in constant time with Vose's alias method, which pays off when the same weights serve many picks. `Random::weightedIndex` suits weights that change between picks.
class WeightedChoice final {
  public:
    // Throws `std::invalid_argument` when there are no weights, a weight is negative or not finite, or every weight is zero.
    explicit WeightedChoice(std::span<const float> weights);

    [[nodiscard]] std::size_t pick(Random& random) const noexcept;
    [[nodiscard]] float getProbability(std::size_t index) const noexcept;

    [[nodiscard]] std::size_t size() const noexcept {
        return probabilities.size();
    }

  private:
    std::vector<float> probabilities;
    std::vector<float> thresholds;
    std::vector<std::uint32_t> aliases;
};

} // namespace haylen::math
