#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "haylen/math/Box.hpp"
#include "haylen/math/Frustum.hpp"
#include "haylen/math/Mat4.hpp"
#include "haylen/math/MatrixBatch.hpp"
#include "haylen/math/Quat.hpp"
#include "haylen/math/Random.hpp"
#include "haylen/math/Vec3.hpp"
#include "math/BatchKernels.hpp"
#include "math/Float4.hpp"
#include "math/ScalarFloat4.hpp"

namespace haylen::math {

// The batches run on the vector instructions of the build and on one lane at a time, and both match the scalar math of `Mat4` and `Frustum` within the tolerance of rounding.
class BatchMathTest : public ::testing::Test {
  protected:
    static constexpr float kTolerance = 1e-5F;

    // Placements of every kind, with rotations, uneven scales and moves, plus the projection of a camera.
    static std::vector<Mat4> makeMatrices(std::size_t count, std::uint32_t seed) {
        Random random(seed);
        std::vector<Mat4> matrices;
        for (std::size_t index = 0; index < count; ++index) {
            const Vec3 axis{random.range(-1.0F, 1.0F), random.range(-1.0F, 1.0F), random.range(0.1F, 1.0F)};
            const Quat rotation = Quat::fromAxisAngle(axis.getNormalized(), random.range(-3.0F, 3.0F));
            const Vec3 translation{random.range(-50.0F, 50.0F), random.range(-50.0F, 50.0F), random.range(-50.0F, 50.0F)};
            const Vec3 scale{random.range(0.1F, 4.0F), random.range(0.1F, 4.0F), random.range(0.1F, 4.0F)};
            matrices.push_back(index % 5 == 4 ? Mat4::perspective({.aspect = 1.5F, .farPlane = 200.0F}) : Mat4::compose(translation, rotation, scale));
        }

        return matrices;
    }

    static void expectNear(const Mat4& actual, const Mat4& expected) {
        for (std::size_t element = 0; element < 16; ++element) {
            EXPECT_NEAR(actual.values[element], expected.values[element], kTolerance * std::max(1.0F, std::fabs(expected.values[element]))) << element;
        }
    }

    static void expectNear(Vec3 actual, Vec3 expected) {
        EXPECT_NEAR(actual.x, expected.x, kTolerance * std::max(1.0F, std::fabs(expected.x)));
        EXPECT_NEAR(actual.y, expected.y, kTolerance * std::max(1.0F, std::fabs(expected.y)));
        EXPECT_NEAR(actual.z, expected.z, kTolerance * std::max(1.0F, std::fabs(expected.z)));
    }

    // Whether rounding may decide the side of a box, because it lies within a hair of where it leaves the frustum.
    static bool isOnTheEdge(const Frustum& frustum, const Box& box) {
        return (frustum.classify(box, Frustum::kTolerance - 1e-3F) == Frustum::Containment::Outside) != (frustum.classify(box, Frustum::kTolerance + 1e-3F) == Frustum::Containment::Outside);
    }

    // Checks the indices that a cull wrote against the classification of each box, apart from the boxes on the edge.
    static void expectCullMatchesClassify(const Frustum& frustum, const std::vector<Box>& boxes, const std::vector<std::uint32_t>& visible, std::size_t count) {
        std::vector<bool> kept(boxes.size(), false);
        for (std::size_t index = 0; index < count; ++index) {
            kept[visible[index]] = true;
            EXPECT_TRUE(index == 0 || visible[index - 1] < visible[index]);
        }

        for (std::size_t index = 0; index < boxes.size(); ++index) {
            if (!isOnTheEdge(frustum, boxes[index])) {
                EXPECT_EQ(kept[index], frustum.classify(boxes[index]) != Frustum::Containment::Outside) << index;
            }
        }
    }
};

TEST_F(BatchMathTest, MultipliesMatricesAsTheScalarMathDoes) {
    const std::vector<Mat4> matrices = makeMatrices(103, 3);
    const Mat4 parent = makeMatrices(1, 9).front();
    std::vector<Mat4> results(matrices.size());
    MatrixBatch::multiply(parent, matrices, results);
    std::vector<Mat4> scalar(matrices.size());
    BatchKernels::multiply<ScalarFloat4>(parent, matrices, scalar);
    for (std::size_t index = 0; index < matrices.size(); ++index) {
        expectNear(results[index], parent * matrices[index]);
        expectNear(scalar[index], parent * matrices[index]);
        expectNear(results[index], scalar[index]);
    }

    // The results may take the place of the matrices.
    std::vector<Mat4> inPlace = matrices;
    MatrixBatch::multiply(parent, inPlace, inPlace);
    for (std::size_t index = 0; index < matrices.size(); ++index) {
        expectNear(inPlace[index], results[index]);
    }

    const std::vector<Mat4> others = makeMatrices(matrices.size(), 5);
    std::vector<Mat4> pairs = others;
    MatrixBatch::multiplyPairs(matrices, pairs, pairs);
    std::vector<Mat4> scalarPairs(matrices.size());
    BatchKernels::multiplyPairs<ScalarFloat4>(matrices, others, scalarPairs);
    for (std::size_t index = 0; index < matrices.size(); ++index) {
        expectNear(pairs[index], matrices[index] * others[index]);
        expectNear(scalarPairs[index], matrices[index] * others[index]);
    }

    std::vector<Mat4> none;
    MatrixBatch::multiply(parent, none, none);
    EXPECT_THROW(MatrixBatch::multiply(parent, matrices, std::span<Mat4>(results).first(3)), std::invalid_argument);
    EXPECT_THROW(MatrixBatch::multiplyPairs(matrices, std::span<const Mat4>(others).first(2), results), std::invalid_argument);
    EXPECT_FALSE(MatrixBatch::getInstructions().empty());
    EXPECT_EQ(MatrixBatch::getInstructions(), Float4::kInstructions);
}

TEST_F(BatchMathTest, TransformsPointsAsTheScalarMathDoes) {
    Random random(17);
    std::vector<Vec3> points;
    for (int index = 0; index < 101; ++index) {
        points.push_back({random.range(-20.0F, 20.0F), random.range(-20.0F, 20.0F), random.range(-120.0F, -2.0F)});
    }

    for (const Mat4& matrix : makeMatrices(5, 21)) {
        std::vector<Vec3> results = points;
        MatrixBatch::transformPoints(matrix, results, results);
        std::vector<Vec3> scalar(points.size());
        BatchKernels::transformPoints<ScalarFloat4>(matrix, points, scalar);
        for (std::size_t index = 0; index < points.size(); ++index) {
            expectNear(results[index], matrix.apply(points[index]));
            expectNear(scalar[index], matrix.apply(points[index]));
        }
    }

    std::vector<Vec3> fewer(2);
    EXPECT_THROW(MatrixBatch::transformPoints(Mat4::identity(), points, fewer), std::invalid_argument);
}

// The cull keeps exactly the boxes that the classification of each box does not call outside, on the vector instructions and one lane at a time, for frustums with a far plane and without one.
TEST_F(BatchMathTest, CullsBoxesAsTheClassificationDoes) {
    const Mat4 view = Mat4::lookAt({3.0F, 2.0F, 10.0F}, {0.0F, 0.0F, -20.0F});
    const Frustum bounded = Frustum::fromMatrix(Mat4::perspective({.aspect = 16.0F / 9.0F, .farPlane = 60.0F}) * view, Mat4::DepthRange::ZeroToOne);
    const Frustum endless = Frustum::fromMatrix(Mat4::perspective({.aspect = 16.0F / 9.0F}) * view, Mat4::DepthRange::ZeroToOne);
    ASSERT_EQ(bounded.getPlanes().size(), 6U);
    ASSERT_EQ(endless.getPlanes().size(), 5U);

    Random random(29);
    std::vector<Box> boxes;
    for (int index = 0; index < 999; ++index) {
        const Vec3 center{random.range(-80.0F, 80.0F), random.range(-40.0F, 40.0F), random.range(-120.0F, 30.0F)};
        const Vec3 size{random.range(0.0F, 6.0F), random.range(0.0F, 6.0F), random.range(0.0F, 6.0F)};
        boxes.push_back(Box::fromCenter(center, size));
    }

    for (const Frustum* frustum : {&bounded, &endless}) {
        std::vector<std::uint32_t> visible(boxes.size());
        const std::size_t count = frustum->cull(boxes, visible);
        EXPECT_GT(count, 0U);
        EXPECT_LT(count, boxes.size());
        expectCullMatchesClassify(*frustum, boxes, visible, count);

        std::vector<std::uint32_t> scalar(boxes.size());
        const std::size_t scalarCount = BatchKernels::cull<ScalarFloat4>(frustum->getPlanes(), boxes, scalar, Frustum::kTolerance);
        expectCullMatchesClassify(*frustum, boxes, scalar, scalarCount);
    }

    // A box that rests on the near plane from outside still counts, since the tolerance never cuts what touches a plane.
    const Plane near = bounded.getPlanes()[4];
    const Vec3 touching = near.getClosestPoint({0.0F, 0.0F, 0.0F}) - near.normal * 0.5F;
    const std::vector<Box> resting{Box::fromCenter(touching, {1.0F, 1.0F, 1.0F})};
    std::vector<std::uint32_t> kept(1);
    EXPECT_EQ(bounded.cull(resting, kept), bounded.classify(resting.front()) != Frustum::Containment::Outside ? 1U : 0U);

    std::vector<std::uint32_t> tooFew(3);
    EXPECT_THROW(static_cast<void>(bounded.cull(boxes, tooFew)), std::invalid_argument);
    EXPECT_EQ(bounded.cull({}, tooFew), 0U);
}

} // namespace haylen::math
