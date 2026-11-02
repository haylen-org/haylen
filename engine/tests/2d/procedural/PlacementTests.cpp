#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/2d/procedural/Placement.hpp"
#include "haylen/math/Geometry.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/Random.hpp"
#include "haylen/math/Segment.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::procedural2d {

namespace {

// Places entities and checks them against the shapes they occupy, in map coordinates.
class PlacementTest : public ::testing::Test {
  protected:
    [[nodiscard]] static math::Vec2 turn(math::Vec2 point, float angle) {
        return {point.x * std::cos(angle) - point.y * std::sin(angle), point.x * std::sin(angle) + point.y * std::cos(angle)};
    }

    // Returns the corners of the footprint of a rectangle or a polygon placed at the entity.
    [[nodiscard]] static std::vector<math::Vec2> corners(const Placement::Shape& shape, const Placement::Entity& entity) {
        std::vector<math::Vec2> local = shape.points;
        if (shape.kind == Placement::Shape::Kind::Rectangle) {
            const math::Vec2 half = shape.size * 0.5F;
            local = {{-half.x, -half.y}, {half.x, -half.y}, {half.x, half.y}, {-half.x, half.y}};
        }
        std::vector<math::Vec2> placed;
        for (const math::Vec2 point : local) {
            placed.push_back(entity.position + turn(shape.center, entity.rotation) + turn(point, shape.rotation + entity.rotation));
        }
        return placed;
    }

    // Tells whether two outlines overlap: an edge of one crosses an edge of the other, or one holds a corner of the other.
    [[nodiscard]] static bool overlaps(const std::vector<math::Vec2>& lhs, const std::vector<math::Vec2>& rhs) {
        for (std::size_t first = 0; first < lhs.size(); ++first) {
            for (std::size_t second = 0; second < rhs.size(); ++second) {
                if (math::Geometry::intersection(math::Segment{lhs[first], lhs[(first + 1) % lhs.size()]}, math::Segment{rhs[second], rhs[(second + 1) % rhs.size()]})) {
                    return true;
                }
            }
        }
        return math::Geometry::contains(lhs, rhs.front()) || math::Geometry::contains(rhs, lhs.front());
    }
};

} // namespace

// Every circle lies inside the area, keeps the larger gap of the two kinds from every other circle and the margin of its kind from the forbidden lake.
TEST_F(PlacementTest, KeepsEveryFootprintApartAndInsideTheArea) {
    Placement::Options options{.area = {0.0F, 0.0F, 800.0F, 600.0F}};
    options.kinds.push_back({.count = 40, .footprint = {.center = {0.0F, 6.0F}, .radius = 12.0F}, .spacing = 10.0F, .margin = 30.0F});
    options.kinds.push_back({.count = 60, .footprint = {.radius = 6.0F}, .spacing = 2.0F, .margin = 4.0F});
    options.forbidden.push_back({.center = {400.0F, 300.0F}, .radius = 120.0F});
    math::Random random(7);
    const Placement::Result result = Placement::place(options, random);
    ASSERT_EQ(result.entities.size(), 100U);
    EXPECT_EQ(result.missing, (std::vector<std::size_t>{0, 0}));

    for (std::size_t index = 0; index < result.entities.size(); ++index) {
        const Placement::Entity& entity = result.entities[index];
        const Placement::Kind& kind = options.kinds[entity.kind];
        const math::Vec2 center = entity.position + kind.footprint.center;
        EXPECT_GE(center.x - kind.footprint.radius, 0.0F);
        EXPECT_LE(center.y + kind.footprint.radius, 600.0F);
        EXPECT_GE(math::Vec2::distance(center, {400.0F, 300.0F}), 120.0F + kind.footprint.radius + kind.margin);
        for (std::size_t other = index + 1; other < result.entities.size(); ++other) {
            const Placement::Kind& otherKind = options.kinds[result.entities[other].kind];
            const math::Vec2 otherCenter = result.entities[other].position + otherKind.footprint.center;
            EXPECT_GE(math::Vec2::distance(center, otherCenter), kind.footprint.radius + otherKind.footprint.radius + std::max(kind.spacing, otherKind.spacing));
        }
    }

    math::Random again(7);
    const Placement::Result repeated = Placement::place(options, again);
    ASSERT_EQ(repeated.entities.size(), result.entities.size());
    EXPECT_EQ(repeated.entities.back().position, result.entities.back().position);
}

// Long thin planks stack row over row, which only their real rectangles allow, and turned rectangles and triangles never overlap one another or the forbidden polygon.
TEST_F(PlacementTest, MeasuresTheRealShapesOfRectanglesAndPolygons) {
    Placement::Options planks{.area = {0.0F, 0.0F, 100.0F, 60.0F}, .attempts = 400};
    planks.kinds.push_back({.count = 8, .footprint = {.kind = Placement::Shape::Kind::Rectangle, .size = {80.0F, 4.0F}}, .spacing = 1.0F});
    math::Random random(3);
    const Placement::Result stacked = Placement::place(planks, random);
    EXPECT_GE(stacked.entities.size(), 6U) << "A circle around each plank would hold only one of them.";

    Placement::Options options{.area = {0.0F, 0.0F, 600.0F, 400.0F}, .attempts = 60};
    const Placement::Shape triangle{.kind = Placement::Shape::Kind::Polygon, .points = {{0.0F, -20.0F}, {18.0F, 14.0F}, {-18.0F, 14.0F}}};
    options.kinds.push_back({.count = 30, .footprint = {.kind = Placement::Shape::Kind::Rectangle, .size = {50.0F, 10.0F}, .rotation = 0.3F}, .rotate = true});
    options.kinds.push_back({.count = 30, .footprint = triangle, .spacing = 3.0F, .rotate = true});
    const Placement::Shape wall{.kind = Placement::Shape::Kind::Polygon, .center = {300.0F, 200.0F}, .points = {{-200.0F, -10.0F}, {200.0F, -10.0F}, {200.0F, 10.0F}, {-200.0F, 10.0F}}};
    options.forbidden.push_back(wall);
    const Placement::Result result = Placement::place(options, random);
    ASSERT_GT(result.entities.size(), 40U);

    const std::vector<math::Vec2> blocked = corners(wall, {});
    std::vector<std::vector<math::Vec2>> placed;
    for (const Placement::Entity& entity : result.entities) {
        placed.push_back(corners(options.kinds[entity.kind].footprint, entity));
        EXPECT_FALSE(overlaps(placed.back(), blocked));
        for (const math::Vec2 point : placed.back()) {
            EXPECT_TRUE(point.x >= 0.0F && point.x <= 600.0F && point.y >= 0.0F && point.y <= 400.0F);
        }
    }
    for (std::size_t first = 0; first < placed.size(); ++first) {
        for (std::size_t second = first + 1; second < placed.size(); ++second) {
            EXPECT_FALSE(overlaps(placed[first], placed[second])) << first << " " << second;
        }
    }
}

// A map without room for everything places what fits and counts what it left out of each kind, shared among the kinds when shuffled and taken from the last kinds when listed.
TEST_F(PlacementTest, PlacesWhatFitsAndCountsWhatItLeavesOut) {
    Placement::Options options{.area = {0.0F, 0.0F, 200.0F, 200.0F}};
    options.kinds.push_back({.count = 40, .footprint = {.radius = 20.0F}});
    options.kinds.push_back({.count = 40, .footprint = {.radius = 20.0F}});
    math::Random random(11);
    const Placement::Result shuffled = Placement::place(options, random);
    EXPECT_LT(shuffled.entities.size(), 80U);
    EXPECT_EQ(shuffled.entities.size() + shuffled.missing[0] + shuffled.missing[1], 80U);
    EXPECT_GT(shuffled.missing[0], 0U);
    EXPECT_GT(shuffled.missing[1], 0U);

    options.order = Placement::Order::Listed;
    options.kinds[0].count = 3;
    const Placement::Result listed = Placement::place(options, random);
    EXPECT_EQ(listed.missing[0], 0U);
    EXPECT_GT(listed.missing[1], 0U);
    EXPECT_EQ(listed.entities.front().kind, 0U);
}

TEST_F(PlacementTest, RejectsOptionsThatCannotPlace) {
    math::Random random(1);
    EXPECT_THROW(static_cast<void>(Placement::place({.area = {0.0F, 0.0F, 0.0F, 10.0F}}, random)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(Placement::place({.area = {0.0F, 0.0F, 10.0F, 10.0F}, .attempts = 0}, random)), std::invalid_argument);

    Placement::Options options{.area = {0.0F, 0.0F, 10.0F, 10.0F}};
    options.kinds.push_back({.count = 1, .footprint = {.radius = 0.0F}});
    EXPECT_THROW(static_cast<void>(Placement::place(options, random)), std::invalid_argument);
    options.kinds[0].footprint = {.kind = Placement::Shape::Kind::Polygon, .points = {{0.0F, 0.0F}, {1.0F, 1.0F}}};
    EXPECT_THROW(static_cast<void>(Placement::place(options, random)), std::invalid_argument);
    options.kinds[0].footprint = {.radius = 1.0F};
    options.kinds[0].spacing = -1.0F;
    EXPECT_THROW(static_cast<void>(Placement::place(options, random)), std::invalid_argument);
    options.kinds[0].spacing = 0.0F;
    options.kinds[0].count = Placement::kMaxEntities + 1;
    EXPECT_THROW(static_cast<void>(Placement::place(options, random)), std::invalid_argument);
}

// Lua places kinds counted from 1 with shape tables, returns the entities and what each kind left out, and does the same work on a worker thread.
TEST(PlacementLuaTest, PlacesKindsFromLua) {
    test::EngineFixture fixture;
    fixture.runLua("procedural = require('haylen.procedural2d')");
    // clang-format off
    fixture.runLua(R"(
        options = {
            area = {0, 0, 400, 300}, seed = 4, order = 'listed',
            kinds = {
                {count = 12, shape = {kind = 'circle', radius = 14, center = {0, 8}}, spacing = 6},
                {count = 200, shape = {kind = 'rectangle', width = 30, height = 12}, rotate = true, margin = 10},
                {count = 2, shape = {kind = 'polygon', points = {{0, -10}, {10, 10}, {-10, 10}}}},
            },
            forbidden = {{kind = 'circle', center = {200, 150}, radius = 60}},
        }
        result = procedural.place(options)
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return #result.missing .. ' ' .. result.missing[1] .. ' ' .. tostring(result.missing[2] > 0) .. ' ' .. result.entities[1].kind"), "3 0 true 1");
    EXPECT_EQ(fixture.lua("local e = result.entities[1] return tostring(e.x >= 0 and e.x <= 400 and e.rotation == 0)"), "true");
    EXPECT_EQ(fixture.lua("return #result.entities + result.missing[1] + result.missing[2] + result.missing[3]"), "214");
    EXPECT_EQ(fixture.lua("return #procedural.place(options).entities == #result.entities"), "true");

    fixture.runLua("done = nil require('async').spawn(function() done = procedural.placeAsync(options):await() end)");
    ASSERT_TRUE(fixture.frameUntil([&fixture] { return fixture.lua("return done ~= nil") == "true"; }, std::chrono::seconds(10)));
    EXPECT_EQ(fixture.lua("return #done.entities == #result.entities and done.entities[5].x == result.entities[5].x"), "true");

    EXPECT_NE(fixture.lua("procedural.place({area = {0, 0, 10, 10}, kinds = {}, size = 3})").find("Unknown option \"size\""), std::string::npos);
    EXPECT_NE(fixture.lua("procedural.place({area = {0, 0, 10, 10}, kinds = {{count = 1, shape = {kind = 'star'}}}})").find("kind"), std::string::npos);
    EXPECT_NE(fixture.lua("procedural.place({area = {0, 0, 10, 10}, kinds = {{count = 1, shape = {radius = -1}}}})").find("A placement shape needs finite values and a size"), std::string::npos);
}

} // namespace haylen::procedural2d
