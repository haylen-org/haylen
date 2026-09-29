#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

namespace haylen::math {

// Deterministic xoshiro256** generator. The same seed always produces the same sequence on every platform.
class Random final {
  public:
    explicit Random(std::uint64_t seed = 0x9E3779B97F4A7C15ULL) noexcept;

    void reseed(std::uint64_t seed) noexcept;

    [[nodiscard]] std::uint64_t nextU64() noexcept;
    [[nodiscard]] std::uint32_t nextU32() noexcept;
    [[nodiscard]] float nextFloat() noexcept;
    [[nodiscard]] float range(float minimum, float maximum) noexcept;
    [[nodiscard]] int range(int minimum, int maximum) noexcept;
    [[nodiscard]] bool chance(float probability) noexcept;
    // Throws std::invalid_argument when a weight is negative or not finite, or when no weight is positive.
    [[nodiscard]] std::size_t weightedIndex(std::span<const float> weights);

    template <typename T> void shuffle(std::span<T> values) noexcept {
        for (std::size_t index = values.size(); index > 1; --index) {
            const auto other = static_cast<std::size_t>(nextU64() % index);
            std::swap(values[index - 1], values[other]);
        }
    }

  private:
    [[nodiscard]] static std::uint64_t splitMix(std::uint64_t& seed) noexcept;

    std::array<std::uint64_t, 4> state{};
};

} // namespace haylen::math
