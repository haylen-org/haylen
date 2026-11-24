#pragma once

#include <cstddef>
#include <vector>

#include "haylen/math/Vec3.hpp"

namespace haylen::math {

// A smooth NURBS volume over a block of control points, which bends the space inside it, such as a lattice that deforms a model, evaluated at three parameters that run from 0 to 1, `u` across the columns, `v` along the rows and `w` through the layers of the block.
class NurbsVolume final {
  public:
    // The block holds its points layer after layer and each layer row after row, `columns` in each row and `rows` in each layer. Empty weights give every point a weight of 1, and empty knots make the clamped uniform knots of their direction.
    struct Options {
        std::size_t columns = 0;
        std::size_t rows = 0;
        std::vector<float> weights;
        std::vector<float> knotsU;
        std::vector<float> knotsV;
        std::vector<float> knotsW;
        int degreeU = 3;
        int degreeV = 3;
        int degreeW = 3;
    };

    // Throws `std::invalid_argument` for points that do not fill whole layers of `columns` by `rows`, for weights that are not one finite number above 0 for each point, or for a degree or knots that do not fit their direction.
    NurbsVolume(std::vector<Vec3> controlPoints, Options options);

    [[nodiscard]] const std::vector<Vec3>& getPoints() const noexcept {
        return points;
    }
    [[nodiscard]] std::size_t getColumns() const noexcept {
        return columns;
    }
    [[nodiscard]] std::size_t getRows() const noexcept {
        return rows;
    }
    [[nodiscard]] std::size_t getLayers() const noexcept {
        return layers;
    }

    [[nodiscard]] Vec3 getPoint(float u, float v, float w) const noexcept;

  private:
    std::vector<Vec3> points;
    std::size_t columns;
    std::size_t rows;
    std::size_t layers;
    int degreeU;
    int degreeV;
    int degreeW;
    std::vector<float> weights;
    std::vector<float> knotsU;
    std::vector<float> knotsV;
    std::vector<float> knotsW;
};

} // namespace haylen::math
