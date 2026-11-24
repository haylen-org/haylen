#pragma once

#include <cstddef>
#include <vector>

#include "haylen/math/Vec3.hpp"

namespace haylen::math {

// A smooth NURBS surface over a grid of control points, such as a hull, a terrain patch or a cloth, evaluated at two parameters that run from 0 to 1, `u` across the columns of the grid and `v` along its rows.
class NurbsSurface final {
  public:
    // The grid holds its points row after row, `columns` in each row. Empty weights give every point a weight of 1, and empty knots make the clamped uniform knots of their direction, which reach the edges of the grid.
    struct Options {
        std::size_t columns = 0;
        std::vector<float> weights;
        std::vector<float> knotsU;
        std::vector<float> knotsV;
        int degreeU = 3;
        int degreeV = 3;
    };

    // The point and its derivatives along `u` and along `v`, which lie in the plane that touches the surface there.
    struct Derivatives {
        Vec3 point;
        Vec3 alongU;
        Vec3 alongV;
    };

    // Throws `std::invalid_argument` for points that do not fill whole rows of `columns`, for weights that are not one finite number above 0 for each point, or for a degree or knots that do not fit their direction.
    NurbsSurface(std::vector<Vec3> controlPoints, Options options);

    [[nodiscard]] const std::vector<Vec3>& getPoints() const noexcept {
        return points;
    }
    [[nodiscard]] std::size_t getColumns() const noexcept {
        return columns;
    }
    [[nodiscard]] std::size_t getRows() const noexcept {
        return rows;
    }
    [[nodiscard]] int getDegreeU() const noexcept {
        return degreeU;
    }
    [[nodiscard]] int getDegreeV() const noexcept {
        return degreeV;
    }
    [[nodiscard]] const std::vector<float>& getWeights() const noexcept {
        return weights;
    }
    [[nodiscard]] const std::vector<float>& getKnotsU() const noexcept {
        return knotsU;
    }
    [[nodiscard]] const std::vector<float>& getKnotsV() const noexcept {
        return knotsV;
    }

    [[nodiscard]] Vec3 getPoint(float u, float v) const noexcept;
    [[nodiscard]] Derivatives getDerivatives(float u, float v) const noexcept;
    // Returns the unit normal, the derivative along `u` crossed with the one along `v`. Throws `std::invalid_argument` where the surface folds onto a line or a point, so it has no normal.
    [[nodiscard]] Vec3 getNormal(float u, float v) const;

  private:
    std::vector<Vec3> points;
    std::size_t columns;
    std::size_t rows;
    int degreeU;
    int degreeV;
    std::vector<float> weights;
    std::vector<float> knotsU;
    std::vector<float> knotsV;
};

} // namespace haylen::math
