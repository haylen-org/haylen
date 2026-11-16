#include "NativeTestValues.hpp"

#include <thread>

std::int32_t NativeTestValues::add(std::int32_t a, std::int32_t b) {
    return a + b;
}

double NativeTestValues::scale(double value, double factor) {
    return value * factor;
}

NativeTestValues::Point NativeTestValues::addPoints(Point a, Point b) {
    return Point{.x = a.x + b.x, .y = a.y + b.y};
}

void NativeTestValues::growRect(Rect& rect, float amount) {
    const auto shift = static_cast<std::int32_t>(amount);
    rect.origin.x -= shift;
    rect.origin.y -= shift;
    rect.width += amount * 2.0F;
    rect.height += amount * 2.0F;
}

void NativeTestValues::fill(std::span<std::uint8_t> buffer, std::uint8_t seed) {
    for (std::size_t index = 0; index < buffer.size(); ++index) {
        buffer[index] = static_cast<std::uint8_t>(seed + index);
    }
}

std::uint32_t NativeTestValues::checksum(std::span<const std::uint8_t> buffer) {
    std::uint32_t hash = 2166136261U;
    for (const std::uint8_t byte : buffer) {
        hash = (hash ^ byte) * 16777619U;
    }

    return hash;
}

std::int32_t NativeTestValues::visit(std::int32_t count, Visitor visitor) {
    for (std::int32_t index = 0; index < count; ++index) {
        const auto label = static_cast<std::size_t>(index) % kLabels.size();
        visitor(index, kLabels[label]);
    }

    return count;
}

void NativeTestValues::reportLater(Reporter reporter, std::int32_t value) {
    std::thread worker(&NativeTestValues::report, reporter, value);
    worker.detach();
}

void NativeTestValues::report(Reporter reporter, std::int32_t value) {
    const std::array<std::uint8_t, 4> bytes{1, 2, 3, 4};
    reporter(value, bytes.data(), bytes.size());
}
