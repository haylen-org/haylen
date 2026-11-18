#include "haylen/math/SignalFilterHelper.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace haylen::math {

void SignalFilterHelper::checkDimensions(int dimensions) {
    if (dimensions < 1 || dimensions > kMaxDimensions) {
        throw std::invalid_argument("A filter has from 1 to " + std::to_string(kMaxDimensions) + " dimensions, not " + std::to_string(dimensions) + ".");
    }
}

void SignalFilterHelper::checkWindow(int window, int maximum) {
    if (window < 1 || window > maximum) {
        throw std::invalid_argument("The window of a filter holds from 1 to " + std::to_string(maximum) + " updates, not " + std::to_string(window) + ".");
    }
}

void SignalFilterHelper::checkValues(std::span<const float> values) {
    if (!std::ranges::all_of(values, [](float value) { return std::isfinite(value); })) {
        throw std::invalid_argument("The values of a filter must be finite numbers.");
    }
}

} // namespace haylen::math
