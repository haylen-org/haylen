#include "haylen/ai/UtilitySelector.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "haylen/math/Math.hpp"
#include "haylen/math/Random.hpp"

namespace haylen::ai {

std::size_t UtilitySelector::add(Option option) {
    for (const Consideration& consideration : option.considerations) {
        if (!consideration.input || consideration.minimum == consideration.maximum) {
            throw std::invalid_argument("A consideration needs an input and a range with two different ends.");
        }
    }
    options.push_back(std::move(option));
    return options.size() - 1;
}

// The compensation of Dave Mark raises each factor toward 1 by a share that grows with the factor count, so a product of many good factors stays good.
float UtilitySelector::score(std::size_t index) const {
    const Option& option = options.at(index);
    const std::size_t count = option.considerations.size();
    const float modification = count > 0 ? 1.0F - 1.0F / static_cast<float>(count) : 0.0F;

    float product = 1.0F;
    for (const Consideration& consideration : option.considerations) {
        const float factor = consideration.curve.evaluate(math::Math::inverseLerp(consideration.minimum, consideration.maximum, consideration.input()));
        if (factor <= 0.0F) {
            return 0.0F;
        }
        product *= factor + (1.0F - factor) * modification * factor;
    }
    return product * option.weight;
}

std::optional<UtilitySelector::Choice> UtilitySelector::choose() const {
    std::optional<Choice> best;
    for (std::size_t index = 0; index < options.size(); ++index) {
        const float value = score(index);
        if (value > 0.0F && (!best || value > best->score)) {
            best = Choice{index, value};
        }
    }
    return best;
}

std::optional<UtilitySelector::Choice> UtilitySelector::choose(math::Random& random, float tolerance) const {
    std::vector<float> scores;
    float highest = 0.0F;
    for (std::size_t index = 0; index < options.size(); ++index) {
        scores.push_back(score(index));
        highest = std::max(highest, scores.back());
    }

    const float threshold = highest * std::clamp(tolerance, 0.0F, 1.0F);
    for (float& value : scores) {
        value = value > 0.0F && value >= threshold ? value : 0.0F;
    }
    if (std::ranges::none_of(scores, [](float value) { return value > 0.0F; })) {
        return std::nullopt;
    }
    const std::size_t index = random.weightedIndex(scores);
    return Choice{index, scores[index]};
}

} // namespace haylen::ai
