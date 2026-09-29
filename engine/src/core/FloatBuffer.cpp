#include "haylen/core/FloatBuffer.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace haylen::core {

FloatBuffer::FloatBuffer(std::size_t count, float value) : values(count, value) {}

float FloatBuffer::get(std::size_t index) const {
    if (index >= values.size()) {
        throw std::out_of_range("Float buffer index " + std::to_string(index) + " is past its size of " + std::to_string(values.size()) + ".");
    }
    return values[index];
}

void FloatBuffer::set(std::size_t index, float value) {
    if (index >= values.size()) {
        throw std::out_of_range("Float buffer index " + std::to_string(index) + " is past its size of " + std::to_string(values.size()) + ".");
    }
    values[index] = value;
}

void FloatBuffer::set(std::size_t first, std::span<const float> source) {
    if (first > values.size() || source.size() > values.size() - first) {
        throw std::out_of_range("Writing " + std::to_string(source.size()) + " values at " + std::to_string(first) + " goes past the float buffer size of " + std::to_string(values.size()) + ".");
    }
    std::ranges::copy(source, values.begin() + static_cast<std::ptrdiff_t>(first));
}

void FloatBuffer::fill(float value) noexcept {
    std::ranges::fill(values, value);
}

} // namespace haylen::core
