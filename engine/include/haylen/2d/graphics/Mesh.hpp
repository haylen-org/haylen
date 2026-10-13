#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "haylen/2d/graphics/MeshVertex.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Rect.hpp"

namespace haylen::graphics2d {

// Triangles of a texture kept between draws, whose vertices change in place, such as cloth, jelly, a flag or a surface bent every frame, so a deformation moves the vertices in one call instead of building the mesh again.
class Mesh final {
  public:
    // Throws `std::invalid_argument` for a count of indices that is not a multiple of three or an index past the last vertex.
    Mesh(graphics::Texture image, std::vector<MeshVertex> points, std::vector<std::uint32_t> triangles);

    // A grid of cells over the area, whose texture coordinates cover the source region of the texture or the whole texture, the start of a free-form deformation. Throws `std::invalid_argument` for fewer than one column or row or more than `kMaxGridCells` cells.
    [[nodiscard]] static Mesh grid(graphics::Texture image, int columns, int rows, const math::Rect& area, std::optional<math::Rect> source = std::nullopt);

    static constexpr int kMaxGridCells = 1 << 20;

    [[nodiscard]] const graphics::Texture& getTexture() const noexcept {
        return texture;
    }
    void setTexture(graphics::Texture value) {
        texture = std::move(value);
    }
    [[nodiscard]] std::span<MeshVertex> getVertices() noexcept {
        return vertices;
    }
    [[nodiscard]] std::span<const MeshVertex> getVertices() const noexcept {
        return vertices;
    }
    [[nodiscard]] std::span<const std::uint32_t> getIndices() const noexcept {
        return indices;
    }
    void setIndices(std::vector<std::uint32_t> value);

    // Moves the vertices from `first` on to the x and y pairs of the values, and copies their positions into the values, which deform a mesh or read its rest pose in one call. Throws `std::out_of_range` for pairs past the last vertex.
    void setPositions(std::span<const float> values, std::size_t first = 0);
    void readPositions(std::span<float> values, std::size_t first = 0) const;

    // Returns the rectangle around every vertex.
    [[nodiscard]] math::Rect getBounds() const noexcept;

  private:
    void checkIndices() const;
    void checkPairs(std::size_t count, std::size_t first) const;

    graphics::Texture texture;
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
};

} // namespace haylen::graphics2d
