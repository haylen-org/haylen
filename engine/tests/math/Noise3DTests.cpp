#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <vector>

#include "haylen/math/Box.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/Noise3D.hpp"
#include "haylen/math/PoissonDisk3D.hpp"
#include "haylen/math/Random.hpp"
#include "haylen/math/Vec3.hpp"
#include "haylen/math/Vec4.hpp"

namespace haylen::math {

class Noise3DTest : public ::testing::Test {
  protected:
    // Returns points spread through a box of many cells, the same on every platform.
    static std::vector<Vec3> probes() {
        std::vector<Vec3> points;
        for (int index = 0; index < 2000; ++index) {
            const auto step = static_cast<float>(index);
            points.push_back({step * 0.173F - 40.0F, step * 0.091F + 3.5F, std::fmod(step * 0.377F, 57.0F) - 12.0F});
        }

        return points;
    }
};

// Every noise stays in its range, repeats itself for the same seed and changes with another seed.
TEST_F(Noise3DTest, StaysInRangeAndRepeatsForASeed) {
    const Noise3D noise(7);
    const Noise3D same(7);
    const Noise3D other(8);
    float spread = 0.0F;
    int differences = 0;
    for (const Vec3 point : probes()) {
        const Vec4 point4 = Vec4::fromVec3(point, point.x * 0.5F);
        for (const float sample : {noise.perlin(point), noise.simplex(point), noise.value(point), noise.perlin(point4), noise.simplex(point4), noise.value(point4)}) {
            EXPECT_GE(sample, -1.0F);
            EXPECT_LE(sample, 1.0F);
            spread = std::max(spread, std::fabs(sample));
        }

        EXPECT_EQ(noise.simplex(point), same.simplex(point));
        EXPECT_EQ(noise.perlin(point4), same.perlin(point4));
        differences += noise.simplex(point) != other.simplex(point) ? 1 : 0;
    }

    EXPECT_GT(spread, 0.6F);
    EXPECT_GT(differences, 1900);
}

// Gradient noise is zero on the corners of its lattice and value noise takes the value of the corner there, and all of them change smoothly between corners.
TEST_F(Noise3DTest, PassesItsLatticeAndChangesSmoothly) {
    const Noise3D noise(3);
    EXPECT_EQ(noise.perlin(Vec3{4.0F, -2.0F, 9.0F}), 0.0F);
    EXPECT_EQ(noise.perlin(Vec4{4.0F, -2.0F, 9.0F, 1.0F}), 0.0F);
    for (const Vec3 point : probes()) {
        const Vec3 nearby = point + Vec3{1e-3F, -1e-3F, 1e-3F};
        EXPECT_NEAR(noise.perlin(point), noise.perlin(nearby), 2e-2F);
        EXPECT_NEAR(noise.simplex(point), noise.simplex(nearby), 2e-2F);
        EXPECT_NEAR(noise.value(point), noise.value(nearby), 2e-2F);
    }
}

TEST_F(Noise3DTest, MeasuresTheCellsOfWorleyNoise) {
    const Noise3D noise(11);
    for (const Vec3 point : probes()) {
        const Noise3D::Cellular cell = noise.cellular(point);
        EXPECT_GE(cell.nearest, 0.0F);
        EXPECT_LE(cell.nearest, cell.second);
        EXPECT_LT(cell.nearest, std::sqrt(3.0F));
        EXPECT_GE(cell.cell, 0.0F);
        EXPECT_LT(cell.cell, 1.0F);

        const Noise3D::Cellular cell4 = noise.cellular(Vec4::fromVec3(point, 2.5F));
        EXPECT_LE(cell4.nearest, cell4.second);
        EXPECT_LT(cell4.nearest, 2.0F);
    }
}

// Each fractal shapes its octaves its own way, and every one stays in its range.
TEST_F(Noise3DTest, SumsFractalsOfEveryShape) {
    const Noise3D noise(5);
    for (const Noise3D::Fractal fractal : {Noise3D::Fractal::Fbm, Noise3D::Fractal::Billow, Noise3D::Fractal::Ridged, Noise3D::Fractal::PingPong}) {
        for (const Noise3D::Basis basis : {Noise3D::Basis::Perlin, Noise3D::Basis::Simplex, Noise3D::Basis::Value}) {
            const Noise3D::FractalOptions options{.basis = basis, .fractal = fractal, .octaves = 5};
            for (const Vec3 point : probes()) {
                const float sample = noise.fractal(point, options);
                EXPECT_GE(sample, -1.0F - 1e-5F);
                EXPECT_LE(sample, 1.0F + 1e-5F);
                EXPECT_LE(std::fabs(noise.fractal(Vec4::fromVec3(point, 0.3F), options)), 1.0F + 1e-5F);
            }
        }
    }

    // One octave of fBm is the basis itself.
    const Vec3 point{1.3F, 2.7F, -0.4F};
    EXPECT_FLOAT_EQ(noise.fractal(point, {.octaves = 1}), noise.simplex(point));
    EXPECT_FLOAT_EQ(noise.fractal(point, {.basis = Noise3D::Basis::Value, .fractal = Noise3D::Fractal::Billow, .octaves = 1}), std::fabs(noise.value(point)) * 2.0F - 1.0F);
    EXPECT_THROW(static_cast<void>(noise.fractal(point, {.octaves = 0})), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(noise.fractal(point, {.octaves = 17})), std::invalid_argument);
    EXPECT_EQ(Noise3D::fractalFromName("pingPong"), Noise3D::Fractal::PingPong);
    EXPECT_EQ(Noise3D::basisName(Noise3D::Basis::Value), "value");
}

TEST_F(Noise3DTest, WarpsPointsWithinTheirAmplitude) {
    const Noise3D noise(9);
    for (const Vec3 point : probes()) {
        const Vec3 warped = noise.warp(point, 2.0F, 0.1F);
        EXPECT_LE(std::fabs(warped.x - point.x), 2.0F + 1e-4F);
        EXPECT_LE(std::fabs(warped.y - point.y), 2.0F + 1e-4F);
        EXPECT_LE(std::fabs(warped.z - point.z), 2.0F + 1e-4F);
    }

    const Vec3 still{3.0F, 4.0F, 5.0F};
    EXPECT_EQ(noise.warp(still, 0.0F), still);
    EXPECT_THROW(static_cast<void>(noise.warp(still, 1.0F, 1.0F, 40)), std::invalid_argument);
}

// Random points land in their shapes and spread over them, the same for the same seed.
TEST_F(Noise3DTest, SpreadsRandomPointsInSpheresConesAndBoxes) {
    Random random(21);
    Random again(21);
    Vec3 sum;
    for (int index = 0; index < 4000; ++index) {
        const Vec3 inside = random.inSphere(2.0F);
        EXPECT_LE(inside.getLength(), 2.0F + 1e-5F);
        EXPECT_EQ(inside, again.inSphere(2.0F));
        const Vec3 surface = random.onSphere(3.0F);
        EXPECT_NEAR(surface.getLength(), 3.0F, 1e-4F);
        static_cast<void>(again.onSphere(3.0F));
        sum += surface;
    }

    // The points of the sphere balance out around its center.
    EXPECT_LT((sum / 4000.0F).getLength(), 0.2F);

    const Vec3 axis = Vec3{1.0F, 1.0F, 0.0F}.getNormalized();
    for (int index = 0; index < 1000; ++index) {
        const Vec3 direction = random.inCone(axis, 0.3F);
        EXPECT_NEAR(direction.getLength(), 1.0F, 1e-4F);
        EXPECT_GE(Vec3::dot(direction, axis), std::cos(0.3F) - 1e-4F);
        EXPECT_GE(Vec3::dot(random.inCone(-Vec3::forward(), Math::kPi), Vec3{0.0F, 0.0F, 1.0F}), -1.0F - 1e-5F);
    }

    const Box crate{{-1.0F, 0.0F, 2.0F}, {1.0F, 5.0F, 3.0F}};
    for (int index = 0; index < 1000; ++index) {
        EXPECT_TRUE(crate.contains(random.inBox(crate)));
    }

    EXPECT_THROW(static_cast<void>(random.inCone(Vec3{}, 0.5F)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(random.inCone(axis, 4.0F)), std::invalid_argument);
}

// Poisson points keep their distance, fill the box and repeat for the same seed.
TEST_F(Noise3DTest, SpreadsPoissonPointsThroughABox) {
    const PoissonDisk3D::Options options{.volume = {{0.0F, 0.0F, 0.0F}, {10.0F, 10.0F, 10.0F}}, .minimumDistance = 1.5F};
    Random random(4);
    const std::vector<Vec3> points = PoissonDisk3D::sample(options, random);
    ASSERT_GT(points.size(), 150U);
    for (std::size_t first = 0; first < points.size(); ++first) {
        EXPECT_TRUE(options.volume.contains(points[first]));
        for (std::size_t second = first + 1; second < points.size(); ++second) {
            EXPECT_GE(Vec3::distance(points[first], points[second]), 1.5F - 1e-4F);
        }
    }

    Random again(4);
    EXPECT_EQ(PoissonDisk3D::sample(options, again), points);

    Random kept(4);
    const std::vector<Vec3> upper = PoissonDisk3D::sample({.volume = options.volume, .minimumDistance = 1.5F, .accept = [](Vec3 point) { return point.y > 5.0F; }}, kept);
    ASSERT_FALSE(upper.empty());
    for (const Vec3 point : upper) {
        EXPECT_GT(point.y, 5.0F);
    }

    EXPECT_TRUE(PoissonDisk3D::sample({.volume = {{1.0F, 1.0F, 1.0F}, {0.0F, 0.0F, 0.0F}}, .minimumDistance = 1.0F}, random).empty());
    EXPECT_EQ(PoissonDisk3D::sample({.volume = {}, .minimumDistance = 1.0F}, random), (std::vector<Vec3>{Vec3{}}));
    EXPECT_THROW(static_cast<void>(PoissonDisk3D::sample({.volume = options.volume, .minimumDistance = 1e-4F}, random)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(PoissonDisk3D::sample({.volume = options.volume, .minimumDistance = 1.0F, .attempts = 0}, random)), std::invalid_argument);
}

} // namespace haylen::math
