#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

// The functions of the test library that take and return plain values, structs, buffers, text and callbacks, which Lua calls through `ffi` with the C types that the tests declare.
class NativeTestValues final {
  public:
    struct Point {
        std::int32_t x;
        std::int32_t y;
    };

    struct Rect {
        Point origin;
        float width;
        float height;
    };

    using Visitor = void (*)(std::int32_t index, const char* label);
    using Reporter = void (*)(std::int32_t value, const std::uint8_t* data, std::size_t size);

    [[nodiscard]] static std::int32_t add(std::int32_t a, std::int32_t b);
    [[nodiscard]] static double scale(double value, double factor);
    [[nodiscard]] static Point addPoints(Point a, Point b);
    static void growRect(Rect& rect, float amount);
    static void fill(std::span<std::uint8_t> buffer, std::uint8_t seed);

    // Hashes the bytes with FNV-1a, so the checksum depends on their order.
    [[nodiscard]] static std::uint32_t checksum(std::span<const std::uint8_t> buffer);

    // Calls the visitor on the calling thread once for each index, with a label that repeats every four indexes, and returns the count.
    static std::int32_t visit(std::int32_t count, Visitor visitor);

    // Calls the reporter from a thread of the library with the value and four bytes, after this function returned.
    static void reportLater(Reporter reporter, std::int32_t value);

  private:
    static constexpr std::array<const char*, 4> kLabels{"zero", "one", "two", "three"};

    static void report(Reporter reporter, std::int32_t value);
};
