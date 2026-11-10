#include "haylen/core/JsonNumber.hpp"

#include <fast_float/fast_float.h>

#include <array>
#include <charconv>
#include <cmath>
#include <format>
#include <limits>
#include <stdexcept>

namespace haylen::core {

double JsonNumber::fromFloat(float value) noexcept {
    std::array<char, 32> text{};
    const std::to_chars_result written = std::to_chars(text.data(), text.data() + text.size(), value);
    double shortest = 0.0;
    fast_float::from_chars(text.data(), written.ptr, shortest);
    return shortest;
}

float JsonNumber::toFloat(const Json& value) {
    const auto number = value.get<double>();
    if (std::fabs(number) > static_cast<double>(std::numeric_limits<float>::max())) {
        throw std::invalid_argument(std::format("The number {:.14g} lies outside the numbers from {:.14g} to {:.14g} that the engine holds.", number, static_cast<double>(std::numeric_limits<float>::lowest()), static_cast<double>(std::numeric_limits<float>::max())));
    }
    return static_cast<float>(number);
}

float JsonNumber::toFloat(const Json& object, std::string_view key, float fallback) {
    const auto found = object.find(key);
    return found != object.end() ? toFloat(*found) : fallback;
}

} // namespace haylen::core
