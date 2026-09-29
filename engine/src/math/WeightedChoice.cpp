#include "haylen/math/WeightedChoice.hpp"

#include <cmath>
#include <stdexcept>

#include "haylen/math/Random.hpp"

namespace haylen::math {

WeightedChoice::WeightedChoice(std::span<const float> weights) : probabilities(weights.size()), thresholds(weights.size(), 1.0F), aliases(weights.size()) {
    double total = 0.0;
    for (const float weight : weights) {
        if (!std::isfinite(weight) || weight < 0.0F) {
            throw std::invalid_argument("Weights must be finite and not negative.");
        }
        total += static_cast<double>(weight);
    }
    if (total <= 0.0) {
        throw std::invalid_argument("A weighted choice needs at least one positive weight.");
    }

    // Every column of the alias table holds one unit of probability, split between its own index and one alias.
    const auto count = static_cast<double>(weights.size());
    std::vector<double> scaled(weights.size());
    std::vector<std::uint32_t> small;
    std::vector<std::uint32_t> large;
    for (std::size_t index = 0; index < weights.size(); ++index) {
        probabilities[index] = static_cast<float>(static_cast<double>(weights[index]) / total);
        scaled[index] = static_cast<double>(weights[index]) * count / total;
        (scaled[index] < 1.0 ? small : large).push_back(static_cast<std::uint32_t>(index));
    }

    while (!small.empty() && !large.empty()) {
        const std::uint32_t lighter = small.back();
        small.pop_back();
        const std::uint32_t heavier = large.back();
        large.pop_back();

        thresholds[lighter] = static_cast<float>(scaled[lighter]);
        aliases[lighter] = heavier;
        scaled[heavier] += scaled[lighter] - 1.0;
        (scaled[heavier] < 1.0 ? small : large).push_back(heavier);
    }
    for (const std::uint32_t index : small) {
        aliases[index] = index;
    }
    for (const std::uint32_t index : large) {
        aliases[index] = index;
    }
}

std::size_t WeightedChoice::pick(Random& random) const noexcept {
    const auto column = static_cast<std::size_t>(random.nextU64() % probabilities.size());
    return random.nextFloat() < thresholds[column] ? column : aliases[column];
}

float WeightedChoice::getProbability(std::size_t index) const noexcept {
    return index < probabilities.size() ? probabilities[index] : 0.0F;
}

} // namespace haylen::math
