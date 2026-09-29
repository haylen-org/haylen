#include "haylen/core/JsonNumber.hpp"

#include <fast_float/fast_float.h>

#include <array>
#include <charconv>

namespace haylen::core {

double JsonNumber::fromFloat(float value) noexcept {
    std::array<char, 32> text{};
    const std::to_chars_result written = std::to_chars(text.data(), text.data() + text.size(), value);
    double shortest = 0.0;
    fast_float::from_chars(text.data(), written.ptr, shortest);
    return shortest;
}

} // namespace haylen::core
