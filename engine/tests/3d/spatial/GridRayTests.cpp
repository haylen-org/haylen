#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <vector>

#include "haylen/3d/spatial/Cell.hpp"
#include "haylen/3d/spatial/GridRay.hpp"
#include "haylen/math/Random.hpp"

namespace haylen::spatial3d {

// Walks rays through grids of boxes and checks each cell they cross.
class GridRay3DTest : public ::testing::Test {
  protected:
    struct Step {
        Cell cell;
        float distance = 0.0F;
        math::Vec3 normal;
    };

    [[nodiscard]] static std::vector<Step> walk(const math::Ray3D& ray, math::Vec3 cellSize) {
        std::vector<Step> steps;
        // clang-format off
        GridRay::traverse(ray, cellSize, [&steps](Cell cell, float distance, math::Vec3 normal) {
            steps.push_back({cell, distance, normal});
            return true;
        });
        // clang-format on
        return steps;
    }
};

TEST_F(GridRay3DTest, CrossesTheCellsAlongAnAxisWithTheFacesItEnters) {
    const std::vector<Step> steps = walk(math::Ray3D::between({0.5F, 0.5F, 0.5F}, {0.5F, 0.5F, -2.5F}), {1.0F, 1.0F, 1.0F});
    ASSERT_EQ(steps.size(), 4U);
    EXPECT_EQ(steps[0].cell, (Cell{0, 0, 0}));
    EXPECT_TRUE(steps[0].normal.isZero());
    for (std::size_t index = 1; index < steps.size(); ++index) {
        EXPECT_EQ(steps[index].cell, (Cell{0, 0, -static_cast<int>(index)}));
        EXPECT_FLOAT_EQ(steps[index].distance, static_cast<float>(index) - 0.5F);
        EXPECT_EQ(steps[index].normal, math::Vec3(0.0F, 0.0F, 1.0F));
    }
}

TEST_F(GridRay3DTest, StepsOneAxisAtATimeAndEntersEveryCellOnItsFace) {
    math::Random random(51);
    const math::Vec3 cellSize{0.5F, 2.0F, 1.25F};
    for (int round = 0; round < 200; ++round) {
        const math::Vec3 from{random.range(-20.0F, 20.0F), random.range(-20.0F, 20.0F), random.range(-20.0F, 20.0F)};
        const math::Vec3 to{random.range(-20.0F, 20.0F), random.range(-20.0F, 20.0F), random.range(-20.0F, 20.0F)};
        const math::Ray3D ray = math::Ray3D::between(from, to);
        const std::vector<Step> steps = walk(ray, cellSize);
        ASSERT_FALSE(steps.empty());
        EXPECT_EQ(steps.front().cell, (Cell{static_cast<int>(std::floor(from.x / cellSize.x)), static_cast<int>(std::floor(from.y / cellSize.y)), static_cast<int>(std::floor(from.z / cellSize.z))}));
        EXPECT_EQ(steps.back().cell, (Cell{static_cast<int>(std::floor(to.x / cellSize.x)), static_cast<int>(std::floor(to.y / cellSize.y)), static_cast<int>(std::floor(to.z / cellSize.z))})) << round;

        for (std::size_t index = 1; index < steps.size(); ++index) {
            const Cell before = steps[index - 1].cell;
            const Cell after = steps[index].cell;
            EXPECT_EQ(std::abs(after.x - before.x) + std::abs(after.y - before.y) + std::abs(after.z - before.z), 1) << round;
            EXPECT_GE(steps[index].distance, steps[index - 1].distance) << round;

            // The point where the ray enters a cell lies on the face that its normal names.
            const math::Vec3 entry = ray.at(steps[index].distance);
            const math::Vec3 low{static_cast<float>(after.x) * cellSize.x, static_cast<float>(after.y) * cellSize.y, static_cast<float>(after.z) * cellSize.z};
            const math::Vec3 face = low + math::Vec3{steps[index].normal.x > 0.0F ? cellSize.x : 0.0F, steps[index].normal.y > 0.0F ? cellSize.y : 0.0F, steps[index].normal.z > 0.0F ? cellSize.z : 0.0F};
            const float along = steps[index].normal.x != 0.0F ? entry.x - face.x : (steps[index].normal.y != 0.0F ? entry.y - face.y : entry.z - face.z);
            EXPECT_NEAR(along, 0.0F, 1e-3F) << round;
        }
    }
}

TEST_F(GridRay3DTest, StopsWhenTheVisitSaysSoAndRefusesEndlessRays) {
    int visited = 0;
    // clang-format off
    GridRay::traverse(math::Ray3D::between({0.5F, 0.5F, 0.5F}, {100.5F, 0.5F, 0.5F}), {1.0F, 1.0F, 1.0F}, [&visited](Cell, float, math::Vec3) {
        ++visited;
        return visited < 3;
    });
    // clang-format on
    EXPECT_EQ(visited, 3);

    const auto ignore = [](Cell, float, math::Vec3) { return true; };
    EXPECT_THROW(GridRay::traverse(math::Ray3D{{}, math::Vec3::forward(), std::numeric_limits<float>::infinity()}, {1.0F, 1.0F, 1.0F}, ignore), std::invalid_argument);
    EXPECT_THROW(GridRay::traverse(math::Ray3D::between({}, {1.0F, 0.0F, 0.0F}), {0.0F, 1.0F, 1.0F}, ignore), std::invalid_argument);
    EXPECT_THROW(GridRay::traverse(math::Ray3D::between({}, {1e12F, 0.0F, 0.0F}), {1.0F, 1.0F, 1.0F}, ignore), std::invalid_argument);
}

} // namespace haylen::spatial3d
