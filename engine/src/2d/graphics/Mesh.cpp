#include "haylen/2d/graphics/Mesh.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

namespace haylen::graphics2d {

Mesh::Mesh(graphics::Texture image, std::vector<MeshVertex> points, std::vector<std::uint32_t> triangles) : texture(std::move(image)), vertices(std::move(points)), indices(std::move(triangles)) {
    checkIndices();
}

Mesh Mesh::grid(graphics::Texture image, int columns, int rows, const math::Rect& area, std::optional<math::Rect> source) {
    if (columns < 1 || rows < 1 || static_cast<long long>(columns) * rows > kMaxGridCells) {
        throw std::invalid_argument("A grid mesh needs at least one column and one row and at most 1048576 cells.");
    }
    const math::Vec2 size = image.isValid() ? image.getSize() : math::Vec2{1.0F, 1.0F};
    const math::Rect region = source.value_or(math::Rect{0.0F, 0.0F, size.x, size.y});
    std::vector<MeshVertex> points;
    points.reserve(static_cast<std::size_t>(columns + 1) * static_cast<std::size_t>(rows + 1));
    for (int row = 0; row <= rows; ++row) {
        for (int column = 0; column <= columns; ++column) {
            const math::Vec2 share{static_cast<float>(column) / static_cast<float>(columns), static_cast<float>(row) / static_cast<float>(rows)};
            points.push_back({.position = area.getMin() + area.getSize() * share, .uv = (region.getMin() + region.getSize() * share) / size});
        }
    }
    std::vector<std::uint32_t> triangles;
    triangles.reserve(static_cast<std::size_t>(columns) * static_cast<std::size_t>(rows) * 6U);
    const auto stride = static_cast<std::uint32_t>(columns + 1);
    for (std::uint32_t row = 0; row < static_cast<std::uint32_t>(rows); ++row) {
        for (std::uint32_t column = 0; column < static_cast<std::uint32_t>(columns); ++column) {
            const std::uint32_t corner = row * stride + column;
            triangles.insert(triangles.end(), {corner, corner + 1U, corner + stride + 1U, corner, corner + stride + 1U, corner + stride});
        }
    }
    return {std::move(image), std::move(points), std::move(triangles)};
}

void Mesh::checkIndices() const {
    if (indices.size() % 3 != 0) {
        throw std::invalid_argument("A mesh lists its triangles as three indices each, and " + std::to_string(indices.size()) + " is not a multiple of three.");
    }
    if (std::ranges::any_of(indices, [this](std::uint32_t index) { return index >= vertices.size(); })) {
        throw std::invalid_argument("Mesh index refers to a vertex that does not exist.");
    }
}

void Mesh::setIndices(std::vector<std::uint32_t> value) {
    std::swap(indices, value);
    try {
        checkIndices();
    } catch (...) {
        std::swap(indices, value);
        throw;
    }
}

void Mesh::checkPairs(std::size_t count, std::size_t first) const {
    if (count % 2 != 0 || first > vertices.size() || count / 2 > vertices.size() - first) {
        throw std::out_of_range("The positions of a mesh come in x and y pairs for the vertices it has, " + std::to_string(vertices.size()) + ", from the first one given.");
    }
}

void Mesh::setPositions(std::span<const float> values, std::size_t first) {
    checkPairs(values.size(), first);
    for (std::size_t index = 0; index < values.size() / 2; ++index) {
        vertices[first + index].position = {values[index * 2], values[index * 2 + 1]};
    }
}

void Mesh::readPositions(std::span<float> values, std::size_t first) const {
    checkPairs(values.size(), first);
    for (std::size_t index = 0; index < values.size() / 2; ++index) {
        values[index * 2] = vertices[first + index].position.x;
        values[index * 2 + 1] = vertices[first + index].position.y;
    }
}

math::Rect Mesh::getBounds() const noexcept {
    if (vertices.empty()) {
        return {};
    }
    math::Vec2 low = vertices.front().position;
    math::Vec2 high = low;
    for (const MeshVertex& vertex : vertices) {
        low = math::Vec2::min(low, vertex.position);
        high = math::Vec2::max(high, vertex.position);
    }
    return {low.x, low.y, high.x - low.x, high.y - low.y};
}

} // namespace haylen::graphics2d
