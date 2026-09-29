#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "haylen/ai/ResponseCurve.hpp"

namespace haylen::math {
class Random;
}

namespace haylen::ai {

// Utility AI: scores each option by the product of its considerations and picks the best one, such as attacking when the enemy is close and healthy or fleeing when health runs low.
class UtilitySelector final {
  public:
    // The input is mapped from minimum and maximum to 0 and 1 before the curve.
    struct Consideration {
        std::string name;
        std::function<float()> input;
        float minimum = 0.0F;
        float maximum = 1.0F;
        ResponseCurve curve{};
    };

    struct Option {
        std::string name;
        std::vector<Consideration> considerations;
        float weight = 1.0F;
    };

    struct Choice {
        std::size_t index = 0;
        float score = 0.0F;
    };

    // Returns the index of the option. Throws std::invalid_argument when a consideration has no input or an empty range.
    std::size_t add(Option option);

    // Multiplies the curves of the considerations, makes up for the number of factors so options with many considerations are not punished, and scales by the weight.
    [[nodiscard]] float score(std::size_t index) const;

    // Returns the option with the best score, or nothing when every option scores zero. Earlier options win ties.
    [[nodiscard]] std::optional<Choice> choose() const;

    // Picks at random, weighted by score, among the options that score at least tolerance times the best score, which makes agents less predictable.
    [[nodiscard]] std::optional<Choice> choose(math::Random& random, float tolerance) const;

    [[nodiscard]] const std::vector<Option>& getOptions() const noexcept {
        return options;
    }
    [[nodiscard]] std::size_t size() const noexcept {
        return options.size();
    }

  private:
    std::vector<Option> options;
};

} // namespace haylen::ai
