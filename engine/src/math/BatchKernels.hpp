#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "haylen/math/Box.hpp"
#include "haylen/math/Mat4.hpp"
#include "haylen/math/Plane.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::math {

// The loops of the batch math over four lanes of floats, `Float4` for the vector instructions of the build or `ScalarFloat4` for one lane at a time. They add and multiply in the order of the math of `Mat4` and `Frustum`, so each lane gives what the scalar math gives within rounding.
class BatchKernels final {
  public:
    template <class Lanes> static void multiply(const Mat4& parent, std::span<const Mat4> matrices, std::span<Mat4> results) noexcept {
        const Columns<Lanes> columns = loadColumns<Lanes>(parent);
        for (std::size_t index = 0; index < matrices.size(); ++index) {
            multiplyColumns<Lanes>(columns, matrices[index], results[index]);
        }
    }

    template <class Lanes> static void multiplyPairs(std::span<const Mat4> lhs, std::span<const Mat4> rhs, std::span<Mat4> results) noexcept {
        for (std::size_t index = 0; index < lhs.size(); ++index) {
            const Columns<Lanes> columns = loadColumns<Lanes>(lhs[index]);
            multiplyColumns<Lanes>(columns, rhs[index], results[index]);
        }
    }

    // Maps each point with a `w` of 1 and divides by the `w` it maps to, as `Mat4::apply` does.
    template <class Lanes> static void transformPoints(const Mat4& matrix, std::span<const Vec3> points, std::span<Vec3> results) noexcept {
        const Columns<Lanes> columns = loadColumns<Lanes>(matrix);
        std::array<float, 4> mapped{};
        for (std::size_t index = 0; index < points.size(); ++index) {
            const Vec3 point = points[index];
            const auto x = Lanes::multiply(columns[0], Lanes::splat(point.x));
            const auto y = Lanes::multiply(columns[1], Lanes::splat(point.y));
            const auto z = Lanes::multiply(columns[2], Lanes::splat(point.z));
            Lanes::store(Lanes::add(Lanes::add(Lanes::add(x, y), z), columns[3]), mapped);
            results[index] = Vec3{mapped[0], mapped[1], mapped[2]} / mapped[3];
        }
    }

    // Writes the indices of the boxes that are not wholly outside the planes into `visible` and returns their count. The planes go in two groups of four lanes, and lanes past the last plane repeat the first, which rejects nothing new.
    template <class Lanes> static std::size_t cull(std::span<const Plane> planes, std::span<const Box> boxes, std::span<std::uint32_t> visible, float tolerance) noexcept {
        std::array<std::array<float, 8>, 4> rows{};
        for (std::size_t lane = 0; lane < 8; ++lane) {
            const Plane& plane = planes[lane < planes.size() ? lane : 0];
            rows[0][lane] = plane.normal.x;
            rows[1][lane] = plane.normal.y;
            rows[2][lane] = plane.normal.z;
            rows[3][lane] = plane.distance;
        }

        std::array<std::array<typename Lanes::Value, 7>, 2> groups{};
        for (std::size_t group = 0; group < 2; ++group) {
            for (std::size_t row = 0; row < 4; ++row) {
                groups[group][row] = Lanes::load(std::span<const float>(rows[row]).subspan(group * 4).template first<4>());
            }

            for (std::size_t axis = 0; axis < 3; ++axis) {
                groups[group][4 + axis] = Lanes::absolute(groups[group][axis]);
            }
        }

        const auto margin = Lanes::splat(tolerance);
        const auto zero = Lanes::splat(0.0F);
        std::size_t count = 0;
        for (std::size_t index = 0; index < boxes.size(); ++index) {
            const Vec3 center = boxes[index].getCenter();
            const Vec3 half = boxes[index].getHalfExtents();
            bool outside = false;
            for (const std::array<typename Lanes::Value, 7>& group : groups) {
                const auto along = Lanes::add(Lanes::add(Lanes::multiply(group[0], Lanes::splat(center.x)), Lanes::multiply(group[1], Lanes::splat(center.y))), Lanes::multiply(group[2], Lanes::splat(center.z)));
                const auto distance = Lanes::subtract(along, group[3]);
                const auto reach = Lanes::add(Lanes::add(Lanes::multiply(Lanes::splat(half.x), group[4]), Lanes::multiply(Lanes::splat(half.y), group[5])), Lanes::multiply(Lanes::splat(half.z), group[6]));
                outside = outside || Lanes::anyLess(distance, Lanes::subtract(Lanes::subtract(zero, reach), margin));
            }

            if (!outside) {
                visible[count] = static_cast<std::uint32_t>(index);
                ++count;
            }
        }

        return count;
    }

  private:
    template <class Lanes> using Columns = std::array<typename Lanes::Value, 4>;

    template <class Lanes> [[nodiscard]] static Columns<Lanes> loadColumns(const Mat4& matrix) noexcept {
        const std::span<const float> values(matrix.values);
        return {Lanes::load(values.first<4>()), Lanes::load(values.subspan<4, 4>()), Lanes::load(values.subspan<8, 4>()), Lanes::load(values.subspan<12, 4>())};
    }

    // Each column of the product is the columns of the left matrix weighted by the column of the right one, which the left matrix leaves alone, so the result may be the right matrix itself.
    template <class Lanes> static void multiplyColumns(const Columns<Lanes>& columns, const Mat4& rhs, Mat4& result) noexcept {
        for (std::size_t column = 0; column < 4; ++column) {
            const std::span<const float, 4> weights = std::span<const float>(rhs.values).subspan(column * 4).template first<4>();
            const auto x = Lanes::multiply(columns[0], Lanes::splat(weights[0]));
            const auto y = Lanes::multiply(columns[1], Lanes::splat(weights[1]));
            const auto z = Lanes::multiply(columns[2], Lanes::splat(weights[2]));
            const auto w = Lanes::multiply(columns[3], Lanes::splat(weights[3]));
            Lanes::store(Lanes::add(Lanes::add(Lanes::add(x, y), z), w), std::span<float>(result.values).subspan(column * 4).template first<4>());
        }
    }
};

} // namespace haylen::math
