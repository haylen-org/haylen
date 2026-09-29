#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

#include "haylen/2d/spatial/Bresenham.hpp"
#include "haylen/2d/spatial/CellGrid.hpp"
#include "haylen/2d/spatial/ConnectedComponents.hpp"
#include "haylen/2d/spatial/FieldOfView.hpp"
#include "haylen/2d/spatial/FloodFill.hpp"
#include "haylen/2d/spatial/GridRay.hpp"
#include "haylen/2d/spatial/UnionFind.hpp"
#include "haylen/2d/spatial/VisibilityPolygon.hpp"
#include "haylen/math/Geometry.hpp"
#include "haylen/math/Random.hpp"
#include "support/EngineFixture.hpp"

namespace haylen {

using Cells = std::vector<spatial2d::Cell>;

TEST(GridRayTest, WalksCrossedCellsInOrder) {
    Cells visited;
    std::vector<math::Vec2> normals;
    // clang-format off
    spatial2d::GridRay::traverse(math::Ray::between({5.0F, 5.0F}, {35.0F, 17.0F}), {10.0F, 10.0F}, [&](spatial2d::Cell cell, float, math::Vec2 normal) {
        visited.push_back(cell);
        normals.push_back(normal);
        return true;
    });
    // clang-format on
    EXPECT_EQ(visited, (Cells{{0, 0}, {1, 0}, {1, 1}, {2, 1}, {3, 1}}));
    EXPECT_TRUE(normals[0].isZero());
    EXPECT_EQ(normals[1], (math::Vec2{-1.0F, 0.0F}));
    EXPECT_EQ(normals[2], (math::Vec2{0.0F, -1.0F}));

    const auto blocked = [](spatial2d::Cell cell) { return cell.x == 2; };
    const std::optional<spatial2d::GridRay::Hit> hit = spatial2d::GridRay::cast(math::Ray::between({5.0F, 5.0F}, {35.0F, 5.0F}), {10.0F, 10.0F}, blocked);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->cell, (spatial2d::Cell{2, 0}));
    EXPECT_FLOAT_EQ(hit->distance, 15.0F);
    EXPECT_EQ(hit->point, (math::Vec2{20.0F, 5.0F}));
    EXPECT_EQ(hit->normal, (math::Vec2{-1.0F, 0.0F}));
    EXPECT_FALSE(spatial2d::GridRay::cast(math::Ray::between({5.0F, 5.0F}, {15.0F, 5.0F}), {10.0F, 10.0F}, blocked).has_value());

    const std::optional<spatial2d::GridRay::Hit> inside = spatial2d::GridRay::cast(math::Ray::between({25.0F, 5.0F}, {5.0F, 5.0F}), {10.0F, 10.0F}, blocked);
    ASSERT_TRUE(inside.has_value());
    EXPECT_EQ(inside->distance, 0.0F);
    EXPECT_TRUE(inside->normal.isZero());

    EXPECT_THROW((void)spatial2d::GridRay::cast(math::Ray{{0.0F, 0.0F}, {1.0F, 0.0F}}, {10.0F, 10.0F}, blocked), std::invalid_argument);
    EXPECT_THROW((void)spatial2d::GridRay::cast(math::Ray::between({0.0F, 0.0F}, {1.0F, 0.0F}), {0.0F, 10.0F}, blocked), std::invalid_argument);
    EXPECT_THROW((void)spatial2d::GridRay::cast(math::Ray::between({0.0F, 0.0F}, {1e30F, 0.0F}), {10.0F, 10.0F}, blocked), std::invalid_argument);
}

// The distances to the next cell sides add up in double precision, so a ray across more than 2^24 cells, where a float sum stops growing, still walks to its end.
TEST(GridRayTest, WalksRaysLongerThanAFloatSumCanCount) {
    std::int64_t visits = 0;
    spatial2d::Cell last{};
    float lastDistance = 0.0F;
    // clang-format off
    spatial2d::GridRay::traverse(math::Ray{{0.5F, 0.5F}, {1.0F, 0.0F}, 16800000.0F}, {1.0F, 1.0F}, [&](spatial2d::Cell cell, float distance, math::Vec2) {
        ++visits;
        last = cell;
        lastDistance = distance;
        return true;
    });
    // clang-format on
    EXPECT_EQ(visits, 16800001);
    EXPECT_EQ(last, (spatial2d::Cell{16800000, 0}));
    EXPECT_FLOAT_EQ(lastDistance, 16799999.5F);
}

TEST(GridRayTest, CastsOverCellGridsFromInsideAndOutside) {
    spatial2d::CellGrid grid(8, 8);
    grid.set({5, 3}, 1);
    const math::Vec2 cellSize{16.0F, 16.0F};

    const std::optional<spatial2d::GridRay::Hit> hit = spatial2d::GridRay::cast(math::Ray{{8.0F, 56.0F}, {1.0F, 0.0F}}, cellSize, grid);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->cell, (spatial2d::Cell{5, 3}));
    EXPECT_FLOAT_EQ(hit->distance, 72.0F);

    // A ray from outside enters through the side of the grid and keeps its full distance.
    const std::optional<spatial2d::GridRay::Hit> fromOutside = spatial2d::GridRay::cast(math::Ray{{-100.0F, 56.0F}, {1.0F, 0.0F}}, cellSize, grid);
    ASSERT_TRUE(fromOutside.has_value());
    EXPECT_FLOAT_EQ(fromOutside->distance, 180.0F);
    EXPECT_EQ(fromOutside->normal, (math::Vec2{-1.0F, 0.0F}));

    grid.set({0, 3}, 2);
    const std::optional<spatial2d::GridRay::Hit> edge = spatial2d::GridRay::cast(math::Ray{{-100.0F, 56.0F}, {1.0F, 0.0F}}, cellSize, grid);
    ASSERT_TRUE(edge.has_value());
    EXPECT_EQ(edge->cell, (spatial2d::Cell{0, 3}));
    EXPECT_EQ(edge->normal, (math::Vec2{-1.0F, 0.0F}));
    EXPECT_FALSE(spatial2d::GridRay::cast(math::Ray{{-100.0F, 200.0F}, {1.0F, 0.0F}}, cellSize, grid).has_value());
    EXPECT_FALSE(spatial2d::GridRay::cast(math::Ray{{24.0F, 8.0F}, {0.0F, 1.0F}}, cellSize, grid).has_value());

    // An invalid cell size raises even when the ray misses the grid it would describe.
    EXPECT_THROW((void)spatial2d::GridRay::cast(math::Ray{{8.0F, 56.0F}, {1.0F, 0.0F}}, {-16.0F, 16.0F}, grid), std::invalid_argument);
}

TEST(BresenhamTest, DrawsLinesAndCircles) {
    Cells cells;
    spatial2d::Bresenham::line({0, 0}, {5, 2}, cells);
    EXPECT_EQ(cells, (Cells{{0, 0}, {1, 0}, {2, 1}, {3, 1}, {4, 2}, {5, 2}}));
    spatial2d::Bresenham::line({3, 3}, {3, -1}, cells);
    EXPECT_EQ(cells.size(), 5U);
    EXPECT_EQ(cells.back(), (spatial2d::Cell{3, -1}));
    spatial2d::Bresenham::line({2, 2}, {2, 2}, cells);
    EXPECT_EQ(cells, (Cells{{2, 2}}));

    spatial2d::Bresenham::circle({10, 10}, 0, cells);
    EXPECT_EQ(cells, (Cells{{10, 10}}));
    spatial2d::Bresenham::circle({0, 0}, 6, cells);
    EXPECT_EQ(cells.front(), (spatial2d::Cell{-2, -6}));
    for (const spatial2d::Cell cell : cells) {
        EXPECT_NEAR(std::hypot(static_cast<float>(cell.x), static_cast<float>(cell.y)), 6.0F, 0.75F);
    }
    Cells unique = cells;
    unique.erase(std::ranges::unique(unique).begin(), unique.end());
    EXPECT_EQ(unique.size(), cells.size());
    EXPECT_THROW(spatial2d::Bresenham::circle({0, 0}, -1, cells), std::invalid_argument);
}

TEST(FieldOfViewTest, SeesOpenRoomsAndHidesCellsBehindWalls) {
    spatial2d::CellGrid grid(21, 21);
    for (int y = 0; y < 21; ++y) {
        grid.set({14, y}, y == 10 ? 0 : 1);
    }

    std::vector<std::uint8_t> seen(21 * 21, 0);
    // clang-format off
    spatial2d::FieldOfView::compute({10, 10}, 6, [&grid](spatial2d::Cell cell) { return grid.isSolid(cell); }, [&](spatial2d::Cell cell) {
        if (grid.contains(cell)) {
            seen[grid.indexOf(cell)] = 1;
        }
    });
    // clang-format on
    EXPECT_EQ(seen[grid.indexOf({10, 10})], 1);
    EXPECT_EQ(seen[grid.indexOf({4, 10})], 1);
    EXPECT_EQ(seen[grid.indexOf({14, 7})], 1);
    EXPECT_EQ(seen[grid.indexOf({16, 10})], 1);
    EXPECT_EQ(seen[grid.indexOf({15, 13})], 0);
    EXPECT_EQ(seen[grid.indexOf({4, 4})], 0);
    EXPECT_THROW(spatial2d::FieldOfView::compute({0, 0}, -1, [](spatial2d::Cell) { return false; }, [](spatial2d::Cell) {}), std::invalid_argument);
}

TEST(FieldOfViewTest, IsSymmetricBetweenOpenCells) {
    constexpr int kSize = 24;
    constexpr int kRadius = 9;
    spatial2d::CellGrid grid(kSize, kSize);
    math::Random random(11);
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            grid.set({x, y}, random.chance(0.3F) ? 1 : 0);
        }
    }

    const auto cells = static_cast<std::size_t>(kSize * kSize);
    std::vector<std::uint8_t> sees(cells * cells, 0);
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            if (grid[{x, y}] != 0) {
                continue;
            }
            const std::size_t viewer = grid.indexOf({x, y});
            // clang-format off
            spatial2d::FieldOfView::compute({x, y}, kRadius, [&grid](spatial2d::Cell cell) { return grid.isSolid(cell); }, [&](spatial2d::Cell cell) {
                if (grid.contains(cell)) {
                    sees[viewer * cells + grid.indexOf(cell)] = 1;
                }
            });
            // clang-format on
        }
    }

    int pairs = 0;
    for (std::size_t first = 0; first < cells; ++first) {
        for (std::size_t second = first + 1; second < cells; ++second) {
            if (grid.getValues()[first] != 0 || grid.getValues()[second] != 0) {
                continue;
            }
            EXPECT_EQ(sees[first * cells + second], sees[second * cells + first]) << first << " and " << second;
            pairs += sees[first * cells + second];
        }
    }
    EXPECT_GT(pairs, 1000);
}

TEST(VisibilityPolygonTest, OutlinesWhatAPointSeesAmongWalls) {
    spatial2d::VisibilityPolygon visibility;
    const math::Rect room{0.0F, 0.0F, 100.0F, 100.0F};
    const std::vector<math::Vec2>& open = visibility.compute({50.0F, 50.0F}, {}, room);
    EXPECT_GE(open.size(), 4U);
    EXPECT_NEAR(std::fabs(math::Geometry::signedArea(open)), 10000.0F, 1.0F);

    // A pillar between the origin and the right wall hides a wedge behind it.
    const std::vector<math::Segment> pillar{{{70.0F, 40.0F}, {70.0F, 60.0F}}, {{70.0F, 60.0F}, {80.0F, 60.0F}}, {{80.0F, 60.0F}, {80.0F, 40.0F}}, {{80.0F, 40.0F}, {70.0F, 40.0F}}};
    const std::vector<math::Vec2> shadowed = visibility.compute({50.0F, 50.0F}, pillar, room);
    EXPECT_TRUE(math::Geometry::contains(shadowed, {60.0F, 50.0F}));
    EXPECT_TRUE(math::Geometry::contains(shadowed, {90.0F, 10.0F}));
    EXPECT_FALSE(math::Geometry::contains(shadowed, {95.0F, 50.0F}));
    for (std::size_t index = 1; index < shadowed.size(); ++index) {
        EXPECT_LE((shadowed[index - 1] - math::Vec2{50.0F, 50.0F}).getAngle(), (shadowed[index] - math::Vec2{50.0F, 50.0F}).getAngle() + 1e-5F);
    }
    EXPECT_THROW((void)visibility.compute({150.0F, 50.0F}, pillar, room), std::invalid_argument);

    // Points that are not finite would leave the angles without an order, so they raise.
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const std::vector<math::Segment> broken{{{nan, 40.0F}, {70.0F, 60.0F}}};
    EXPECT_THROW((void)visibility.compute({nan, 50.0F}, pillar, room), std::invalid_argument);
    EXPECT_THROW((void)visibility.compute({50.0F, 50.0F}, broken, room), std::invalid_argument);
    EXPECT_THROW((void)visibility.compute({50.0F, 50.0F}, pillar, {0.0F, 0.0F, std::numeric_limits<float>::infinity(), 100.0F}), std::invalid_argument);
}

TEST(FloodFillTest, FillsConnectedCellsOfTheSameValue) {
    spatial2d::CellGrid grid(5, 5);
    for (int x = 0; x < 5; ++x) {
        grid.set({x, 2}, 1);
    }
    grid.set({2, 2}, 0);
    grid.set({4, 4}, 1);
    grid.set({3, 3}, 1);

    spatial2d::FloodFill fill;
    Cells cells;
    fill.fill(grid, {0, 0}, false, cells);
    EXPECT_EQ(cells.front(), (spatial2d::Cell{0, 0}));
    // The water cell at 4,3 is walled in by land on every side, so it stays out of the fill.
    EXPECT_EQ(cells.size(), 18U);
    fill.fill(grid, {0, 2}, false, cells);
    EXPECT_EQ(cells.size(), 2U);
    fill.fill(grid, {3, 3}, false, cells);
    EXPECT_EQ(cells.size(), 3U);
    fill.fill(grid, {3, 3}, true, cells);
    EXPECT_EQ(cells.size(), 4U);
    fill.fill(grid, {9, 9}, false, cells);
    EXPECT_TRUE(cells.empty());

    // clang-format off
    fill.fill({0, 0}, 3, 3, false, [](spatial2d::Cell cell) { return cell.x + cell.y < 3; }, cells);
    // clang-format on
    EXPECT_EQ(cells.size(), 6U);
}

TEST(ConnectedComponentsTest, NumbersIslandsAndRegions) {
    spatial2d::CellGrid grid(6, 4);
    for (const spatial2d::Cell land : {spatial2d::Cell{0, 0}, {1, 0}, {1, 1}, {3, 1}, {4, 2}, {5, 3}, {0, 3}}) {
        grid.set(land, 1);
    }

    spatial2d::CellGrid labels(6, 4);
    EXPECT_EQ(spatial2d::ConnectedComponents::label(grid, {.background = 0}, labels), 5U);
    EXPECT_EQ((labels[{0, 0}]), 1);
    EXPECT_EQ((labels[{1, 1}]), 1);
    EXPECT_EQ((labels[{3, 1}]), 2);
    EXPECT_EQ((labels[{0, 3}]), 4);
    EXPECT_EQ((labels[{2, 2}]), 0);
    EXPECT_EQ(spatial2d::ConnectedComponents::label(grid, {.diagonal = true, .background = 0}, labels), 3U);
    EXPECT_EQ((labels[{5, 3}]), (labels[{3, 1}]));

    // Without a background every value forms regions, water included.
    EXPECT_EQ(spatial2d::ConnectedComponents::label(grid, {}, labels), 6U);
    spatial2d::CellGrid wrong(2, 2);
    EXPECT_THROW(spatial2d::ConnectedComponents::label(grid, {}, wrong), std::invalid_argument);
}

TEST(UnionFindTest, MergesSetsAndTracksTheirSizes) {
    spatial2d::UnionFind sets(5);
    EXPECT_EQ(sets.getSetCount(), 5U);
    EXPECT_TRUE(sets.unite(0, 1));
    EXPECT_TRUE(sets.unite(3, 4));
    EXPECT_FALSE(sets.unite(1, 0));
    EXPECT_TRUE(sets.unite(1, 4));
    EXPECT_TRUE(sets.isConnected(0, 3));
    EXPECT_FALSE(sets.isConnected(0, 2));
    EXPECT_EQ(sets.getSetSize(3), 4U);
    EXPECT_EQ(sets.getSetCount(), 2U);
    EXPECT_EQ(sets.add(), 5U);
    EXPECT_EQ(sets.size(), 6U);
    EXPECT_EQ(sets.getSetCount(), 3U);
    EXPECT_THROW((void)sets.find(6), std::out_of_range);
    sets.reset(2);
    EXPECT_EQ(sets.getSetCount(), 2U);
}

TEST(GridAlgorithmsLuaTest, RunsGridAlgorithmsFromLua) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        spatial2d = require('haylen.spatial2d')
        grid = spatial2d.newCellGrid(8, 6)
        for row = 0, 5 do grid:set(5, row, 1) end
        grid:set(5, 2, 0)
        function cells(list)
            local parts = {}
            for _, cell in ipairs(list) do parts[#parts + 1] = cell.x .. ':' .. cell.y end
            return table.concat(parts, ' ')
        end
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("return grid.width .. 'x' .. grid.height .. ' ' .. grid:get(5, 0) .. grid:get(5, 2) .. ' ' .. tostring(grid:contains(8, 0))"), "8x6 10 false");
    EXPECT_EQ(fixture.lua("local hit = spatial2d.raycastGrid(grid, {8, 8}, {200, 8}, 16) return hit.column .. ':' .. hit.row .. ' ' .. hit.x .. ' ' .. hit.normalX"), "5:0 80.0 -1.0");
    EXPECT_EQ(fixture.lua("return tostring(spatial2d.raycastGrid(grid, {8, 40}, {200, 40}, {16, 16}))"), "nil");
    EXPECT_NE(fixture.lua("spatial2d.raycastGrid(grid, {8, 40}, {200, 40}, -16)").find("positive and finite size"), std::string::npos);
    EXPECT_EQ(fixture.lua("local crossed = spatial2d.traverseGrid({5, 5}, {35, 17}, 10) return cells(crossed) .. ' ' .. crossed[1].distance .. ' ' .. tostring(crossed[2].distance > 5)"), "0:0 1:0 1:1 2:1 3:1 0.0 true");
    EXPECT_EQ(fixture.lua("return cells(spatial2d.traverseGrid({5, 5}, {5, 25}, {10, 20}))"), "0:0 0:1");
    EXPECT_NE(fixture.lua("spatial2d.traverseGrid({0, 0}, {1000000, 0}, 1)").find("at most 65536 cells"), std::string::npos);
    EXPECT_NE(fixture.lua("spatial2d.traverseGrid({0, 0}, {1e10, 0}, 1)").find("32-bit range of cells"), std::string::npos);
    EXPECT_EQ(fixture.lua("return cells(spatial2d.line(0, 0, 3, 1)) .. ' / ' .. #spatial2d.circle(0, 0, 3)"), "0:0 1:0 2:1 3:1 / 16");
    EXPECT_EQ(fixture.lua("local seen = {} for _, cell in ipairs(spatial2d.fieldOfView(grid, 2, 2, 10)) do seen[cell.x .. ':' .. cell.y] = true end return tostring(seen['7:2']) .. ' ' .. tostring(seen['7:0']) .. ' ' .. tostring(seen['5:0'])"), "true nil true");
    EXPECT_EQ(fixture.lua("return #spatial2d.fieldOfView(grid, 2, 2, 2000000000) == #spatial2d.fieldOfView(grid, 2, 2, 10)"), "true");
    EXPECT_EQ(fixture.lua("return #spatial2d.fieldOfView(grid, -3, 2, 2000000000) .. ' ' .. #spatial2d.fieldOfView(grid, 7, 5, 0)"), "0 1");
    EXPECT_EQ(fixture.lua("local fill = spatial2d.floodFill(grid, 0, 0) return #fill .. ' ' .. #spatial2d.floodFill(grid, 5, 0, {diagonal = true})"), "43 2");
    EXPECT_EQ(fixture.lua("spatial2d.floodFill(grid, 7, 5, {value = 3}) return grid:get(0, 0) .. grid:get(6, 0)"), "33");
    EXPECT_EQ(fixture.lua("local labels, count = spatial2d.components(grid, {background = 3}) return count .. ' ' .. labels:get(5, 0) .. labels:get(5, 3) .. labels:get(0, 0)"), "2 120");
    EXPECT_EQ(fixture.lua("local labels, count = spatial2d.components(grid) return count"), "3");
    EXPECT_EQ(fixture.lua("local outline = spatial2d.visibilityPolygon({50, 50}, {{{70, 40}, {70, 60}}}, {0, 0, 100, 100}) return tostring(#outline >= 6) .. ' ' .. tostring(math.abs(require('haylen.math').polygonSignedArea(outline)) < 10000)"), "true true");

    fixture.runLua("sets = spatial2d.newUnionFind(4)");
    EXPECT_EQ(fixture.lua("return tostring(sets:unite(1, 2)) .. tostring(sets:unite(2, 1)) .. ' ' .. sets:find(2) .. ' ' .. tostring(sets:connected(1, 3)) .. ' ' .. sets:setSize(1) .. ' ' .. sets.setCount .. ' ' .. sets:add() .. ' ' .. sets.size"), "truefalse 1 false 2 3 5 5");
    EXPECT_NE(fixture.lua("sets:find(0)").find("count from 1"), std::string::npos);
    EXPECT_EQ(fixture.lua("sets:reset(3) return sets.size .. ' ' .. sets.setCount .. ' ' .. tostring(sets:connected(1, 2))"), "3 3 false");
    EXPECT_NE(fixture.lua("grid:set(9, 0, 1)").find("outside the cell grid"), std::string::npos);
    EXPECT_NE(fixture.lua("spatial2d.floodFill(grid, 0, 0, {fill = 2})").find("Unknown option 'fill'"), std::string::npos);
    EXPECT_NE(fixture.lua("spatial2d.newCellGrid(0, 3)").find("positive size"), std::string::npos);
}

} // namespace haylen
