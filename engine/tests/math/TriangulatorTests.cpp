#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

#include "haylen/math/Geometry.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/Polygon.hpp"
#include "math/Triangulator.hpp"

namespace haylen::math {

namespace {

// Triangulates shapes and measures what their triangles cover.
class TriangulatorTest : public ::testing::Test {
  protected:
    static Polygon::Outline square(float x, float y, float size, bool reversed = false) {
        Polygon::Outline outline{{x, y}, {x + size, y}, {x + size, y + size}, {x, y + size}};
        if (reversed) {
            std::reverse(outline.begin(), outline.end());
        }
        return outline;
    }

    static Polygon::Outline circle(Vec2 center, float radius, int segments, bool reversed = false) {
        Polygon::Outline outline;
        for (int index = 0; index < segments; ++index) {
            const float turn = Math::kTau * static_cast<float>(index) / static_cast<float>(segments);
            outline.push_back(center + Vec2::fromAngle(reversed ? -turn : turn, radius));
        }
        return outline;
    }

    void triangulate(const std::vector<Polygon::Outline>& shape) {
        points.clear();
        counts.clear();
        for (const Polygon::Outline& outline : shape) {
            points.insert(points.end(), outline.begin(), outline.end());
            counts.push_back(static_cast<std::uint32_t>(outline.size()));
        }
        triangulator.triangulate(points, counts);
    }

    // The area the triangles cover, each of which must turn the positive way.
    [[nodiscard]] float coveredArea() const {
        const std::vector<std::uint32_t>& triangles = triangulator.getTriangles();
        float area = 0.0F;
        for (std::size_t index = 0; index < triangles.size(); index += 3) {
            const std::array<Vec2, 3> triangle{points[triangles[index]], points[triangles[index + 1]], points[triangles[index + 2]]};
            EXPECT_GE(Geometry::signedArea(triangle), 0.0F);
            area += Geometry::signedArea(triangle);
        }
        return area;
    }

    // The area of the shape where every hole winds against the outline around it, so the signed areas of its outlines add up to it.
    [[nodiscard]] float shapeArea() const {
        float area = 0.0F;
        for (std::size_t outline = 0, first = 0; outline < counts.size(); first += counts[outline++]) {
            area += Geometry::signedArea(std::span(points).subspan(first, counts[outline]));
        }
        return area;
    }

    // Tells whether a triangle covers the point.
    [[nodiscard]] bool covers(Vec2 point) const {
        const std::vector<std::uint32_t>& triangles = triangulator.getTriangles();
        for (std::size_t index = 0; index < triangles.size(); index += 3) {
            const std::array<Vec2, 3> triangle{points[triangles[index]], points[triangles[index + 1]], points[triangles[index + 2]]};
            if (Geometry::contains(triangle, point)) {
                return true;
            }
        }
        return false;
    }

    Triangulator triangulator;
    std::vector<Vec2> points;
    std::vector<std::uint32_t> counts;
};

} // namespace

// An outline wound against the outline around it cuts a hole, which no triangle covers, in any winding of the pair.
TEST_F(TriangulatorTest, CutsHolesOutOfTheFill) {
    for (const bool reversed : {false, true}) {
        triangulate({square(0.0F, 0.0F, 100.0F, reversed), square(30.0F, 30.0F, 40.0F, !reversed)});
        EXPECT_NEAR(coveredArea(), 10000.0F - 1600.0F, 0.01F);
        EXPECT_FALSE(covers({50.0F, 50.0F}));
        EXPECT_TRUE(covers({10.0F, 50.0F}));
        EXPECT_EQ(triangulator.getKinds(), (std::vector<Triangulator::Kind>{Triangulator::Kind::Outer, Triangulator::Kind::Hole}));
    }

    // A ring of round outlines, and a disc with three holes, one of which holds an island.
    triangulate({circle({0.0F, 0.0F}, 200.0F, 64), circle({0.0F, 0.0F}, 120.0F, 48, true)});
    EXPECT_NEAR(coveredArea(), shapeArea(), 1.0F);
    EXPECT_FALSE(covers({0.0F, 0.0F}));
    triangulate({circle({0.0F, 0.0F}, 300.0F, 72), circle({-150.0F, 0.0F}, 60.0F, 24, true), circle({150.0F, 0.0F}, 80.0F, 24, true), circle({0.0F, 150.0F}, 50.0F, 20, true), circle({150.0F, 0.0F}, 30.0F, 16)});
    EXPECT_FALSE(covers({-150.0F, 0.0F}));
    EXPECT_FALSE(covers({150.0F, 60.0F}));
    EXPECT_TRUE(covers({150.0F, 0.0F}));
    EXPECT_TRUE(covers({0.0F, -150.0F}));
    EXPECT_NEAR(coveredArea(), shapeArea(), 1.0F);
}

// The nonzero rule fills an outline wound like the outline around it without a hole, leaves out outlines without area, and keeps outlines that only share edges side by side.
TEST_F(TriangulatorTest, FillsByTheNonzeroRule) {
    triangulate({square(0.0F, 0.0F, 100.0F), square(30.0F, 30.0F, 40.0F)});
    EXPECT_NEAR(coveredArea(), 10000.0F, 0.01F);
    EXPECT_TRUE(covers({50.0F, 50.0F}));
    EXPECT_EQ(triangulator.getKinds()[1], Triangulator::Kind::Inside);

    triangulate({square(0.0F, 0.0F, 10.0F), {{0.0F, 0.0F}, {5.0F, 5.0F}}, square(10.0F, 0.0F, 10.0F, true)});
    EXPECT_NEAR(coveredArea(), 200.0F, 0.01F);
    EXPECT_EQ(triangulator.getKinds(), (std::vector<Triangulator::Kind>{Triangulator::Kind::Outer, Triangulator::Kind::Inside, Triangulator::Kind::Outer}));
}

// A hole that touches the outline around it at a vertex, holes side by side and a hole that shares an edge with the outline still fill exactly the area between them.
TEST_F(TriangulatorTest, JoinsHolesThatTouch) {
    triangulate({square(0.0F, 0.0F, 100.0F), square(0.0F, 40.0F, 20.0F, true)});
    EXPECT_NEAR(coveredArea(), 10000.0F - 400.0F, 0.01F);
    triangulate({square(0.0F, 0.0F, 100.0F), square(20.0F, 20.0F, 20.0F, true), square(40.0F, 20.0F, 20.0F, true), square(20.0F, 40.0F, 20.0F, true)});
    EXPECT_NEAR(coveredArea(), 10000.0F - 1200.0F, 0.01F);
    EXPECT_FALSE(covers({50.0F, 30.0F}));
}

// The outlines that polygon booleans return triangulate whole, and an outline that crosses itself ends without a hang.
TEST_F(TriangulatorTest, TriangulatesTheResultsOfBooleans) {
    const std::vector<Polygon::Outline> ring{circle({0.0F, 0.0F}, 220.0F, 48), circle({0.0F, 0.0F}, 110.0F, 48, true)};
    const std::vector<Polygon::Outline> star{circle({-120.0F, 0.0F}, 320.0F, 14)};
    const std::vector<Polygon::Outline> result = Polygon::subtract(star, ring);
    ASSERT_GE(result.size(), 2U);
    triangulate(result);
    EXPECT_NEAR(coveredArea(), Polygon::getArea(result), 2.0F);

    triangulate({{{0.0F, 0.0F}, {10.0F, 10.0F}, {10.0F, 0.0F}, {0.0F, 10.0F}}});
    EXPECT_LE(triangulator.getTriangles().size(), 6U);
}

} // namespace haylen::math
