#pragma once

#include <cstddef>
#include <format>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "haylen/math/Vec3.hpp"

namespace haylen::lua::animation3d {

// Moves the vertices of meshes between the float buffers of Lua, three numbers for each vertex, and the vectors that skinning and morphing take.
class VertexBuffers3D final {
  public:
    // Reads `count` vectors from the numbers, and throws `std::invalid_argument` with a sentence that names `what` when they hold too few.
    static void read(std::span<const float> numbers, std::size_t count, std::vector<haylen::math::Vec3>& target, std::string_view what) {
        requireRoom(numbers, count, what);
        target.resize(count);
        for (std::size_t vertex = 0; vertex < count; ++vertex) {
            target[vertex] = {numbers[vertex * 3], numbers[vertex * 3 + 1], numbers[vertex * 3 + 2]};
        }
    }

    // Writes the vectors into numbers that hold at least three for each of them.
    static void write(const std::vector<haylen::math::Vec3>& source, std::span<float> numbers) {
        for (std::size_t vertex = 0; vertex < source.size(); ++vertex) {
            numbers[vertex * 3] = source[vertex].x;
            numbers[vertex * 3 + 1] = source[vertex].y;
            numbers[vertex * 3 + 2] = source[vertex].z;
        }
    }

    // Throws `std::invalid_argument` with a sentence that names `what` when the numbers hold fewer than three for each of `count` vertices.
    static void requireRoom(std::span<const float> numbers, std::size_t count, std::string_view what) {
        if (numbers.size() < count * 3) {
            throw std::invalid_argument(std::format("The {} of {} vertices take {} values, while the buffer holds {}.", what, count, count * 3, numbers.size()));
        }
    }
};

} // namespace haylen::lua::animation3d
