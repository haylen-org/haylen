#include <gtest/gtest.h>

#include <string>

#include "support/EngineFixture.hpp"

namespace haylen {

class GeometryLuaTest : public ::testing::Test {
  protected:
    void SetUp() override {
        fixture.runLua("m = require('haylen.math') square = {{0, 0}, {10, 0}, {10, 10}, {0, 10}} other = {{5, 5}, {15, 5}, {15, 15}, {5, 15}}");
    }

    std::string lua(const std::string& source) {
        return fixture.lua(source);
    }

    test::EngineFixture fixture;
};

TEST_F(GeometryLuaTest, CombinesAndOffsetsPolygons) {
    EXPECT_EQ(lua("return m.polygon.area(m.polygon.unite(square, other))"), "175.0");
    EXPECT_EQ(lua("return m.polygon.area(m.polygon.subtract(square, other)) .. ' ' .. m.polygon.area(m.polygon.intersect(square, other)) .. ' ' .. m.polygon.area(m.polygon.exclude(square, other))"), "75.0 25.0 150.0");
    EXPECT_EQ(lua("return #m.polygon.unite({square, {{20, 0}, {30, 0}, {30, 10}}})"), "2");
    EXPECT_EQ(lua("local grown = m.polygon.offset(square, 1, {join = 'miter'}) return m.polygon.area(grown)"), "144.0");
    EXPECT_EQ(lua("return #m.polygon.offset(square, -6)"), "0");
    EXPECT_EQ(lua("local ring = m.polygon.subtract(square, {{3, 3}, {7, 3}, {7, 7}, {3, 7}}) return #ring .. ' ' .. m.polygon.area(ring)"), "2 84.0");
    EXPECT_NE(lua("return m.polygon.offset(square, 1, {join = 'sharp'})").find("unknown value 'sharp'"), std::string::npos);
    EXPECT_NE(lua("return m.polygon.offset(square, 1, {limit = 3})").find("Unknown option 'limit'"), std::string::npos);
}

TEST_F(GeometryLuaTest, SimplifiesAndDecomposesPolygons) {
    EXPECT_EQ(lua("return #m.polygon.simplify({{0, 0}, {5, 0.1}, {10, 0}, {10, 10}}, 0.5, false)"), "3");
    EXPECT_EQ(lua("return #m.polygon.simplify({{0, 0}, {5, 0.1}, {10, 0}, {10, 10}, {0, 10}}, 0.5)"), "4");
    // clang-format off
    fixture.runLua(R"(
        dent = {{0, 0}, {40, 0}, {20, 10}, {40, 40}, {0, 40}}
        pieces = m.polygon.decompose(dent)
        total = 0
        convex = true
        for _, piece in ipairs(pieces) do
            total = total + m.polygonSignedArea(piece)
            convex = convex and m.polygonConvex(piece)
        end
    )");
    // clang-format on
    EXPECT_EQ(lua("return #pieces .. ' ' .. total .. ' ' .. tostring(convex)"), "2 1200.0 true");
    EXPECT_EQ(lua("return #m.polygon.decompose(dent, 3)"), "3");
}

TEST_F(GeometryLuaTest, TracesFieldsAndBitmaps) {
    // clang-format off
    fixture.runLua(R"(
        field = {}
        for y = 0, 4 do
            for x = 0, 4 do
                field[#field + 1] = (x >= 1 and x <= 3 and y >= 1 and y <= 3) and 1 or 0
            end
        end
    )");
    // clang-format on
    EXPECT_EQ(lua("local outlines = m.marchingSquares.trace(field, 5, 5, {threshold = 0.5, spacing = 2, origin = {100, 0}}) return #outlines .. ' ' .. m.polygonSignedArea(outlines[1])"), "1 34.0");
    EXPECT_EQ(lua("local outlines = m.marchingSquares.traceBitmap({1, 0, 1, true, false, 0}, 3, 2) return #outlines"), "2");
    EXPECT_NE(lua("return m.marchingSquares.trace(field, 4, 5)").find("width times height"), std::string::npos);
    EXPECT_NE(lua("return m.marchingSquares.trace(field, 5, 5, {level = 1})").find("Unknown option 'level'"), std::string::npos);
}

TEST_F(GeometryLuaTest, SamplesSplines) {
    fixture.runLua("curve = m.spline({{0, 0}, {100, 0}, {100, 100}}) loop = m.spline({{0, 0}, {10, 0}, {10, 10}}, {kind = 'bSpline', closed = true})");
    EXPECT_EQ(lua("return curve.kind .. ' ' .. tostring(curve.closed) .. ' ' .. curve.segmentCount .. ' ' .. #curve.points"), "catmullRom false 2 3");
    EXPECT_EQ(lua("return curve:point(0).x .. ' ' .. math.floor(curve:point(1).y + 0.5)"), "0.0 100");
    EXPECT_EQ(lua("return tostring(curve.length > 200) .. ' ' .. tostring(math.abs(curve:tangent(0):length() - 1) < 1e-4)"), "true true");
    EXPECT_EQ(lua("local points = curve:sampleByDistance(20) return tostring(#points >= 10) .. ' ' .. tostring(points[#points] == curve:point(1))"), "true true");
    EXPECT_EQ(lua("return #curve:sample(5) .. ' ' .. tostring(curve:pointAtDistance(0) == curve:point(0)) .. ' ' .. curve:parameterAtDistance(0)"), "5 true 0.0");
    EXPECT_EQ(lua("return tostring(math.abs(curve:tangentAtDistance(10).x - 1) < 0.1) .. ' ' .. loop.kind .. ' ' .. tostring(loop.closed)"), "true bSpline true");
    EXPECT_NE(lua("return m.spline({{0, 0}, {1, 0}}, {kind = 'bezier'})").find("3n + 1"), std::string::npos);
    EXPECT_NE(lua("return m.spline({{0, 0}, {1, 0}}, {kind = 'nurbs'})").find("unknown value 'nurbs'"), std::string::npos);
}

TEST_F(GeometryLuaTest, SpringsAndSmoothDamping) {
    fixture.runLua("spring = m.spring(0, 0.25) for i = 1, 120 do spring:update(10, 1 / 60) end");
    EXPECT_EQ(lua("return tostring(math.abs(spring.value - 10) < 0.01) .. ' ' .. spring.smoothTime"), "true 0.25");
    EXPECT_EQ(lua("spring.value = 3 spring.velocity = 2 spring.smoothTime = 1 return spring.value .. ' ' .. spring.velocity .. ' ' .. spring.smoothTime"), "3.0 2.0 1.0");
    EXPECT_EQ(lua("local value, velocity = m.smoothDamp(0, 10, 0, 0.3, 1 / 60) return tostring(value > 0 and value < 10) .. ' ' .. tostring(velocity > 0)"), "true true");
    EXPECT_EQ(lua("local value, velocity = m.smoothDamp(m.vec2(0, 0), {10, 0}, m.vec2(), 0.3, 1 / 60) return tostring(value.x > 0) .. ' ' .. tostring(velocity.x > 0) .. ' ' .. value.y"), "true true 0.0");
}

TEST_F(GeometryLuaTest, DealsFromBagsAndWeightedChoices) {
    // clang-format off
    fixture.runLua(R"(
        bag = m.shuffleBag({'sword', 'shield', 'potion'}, {counts = {1, 1, 2}, seed = 4})
        seen = {}
        for i = 1, 4 do
            local item = bag:next()
            seen[item] = (seen[item] or 0) + 1
        end
    )");
    // clang-format on
    EXPECT_EQ(lua("return seen.sword .. seen.shield .. seen.potion .. ' ' .. bag.remaining .. ' ' .. bag.size"), "112 0 4");
    EXPECT_EQ(lua("bag:refill() local rng = m.random(1) bag:next(rng) return bag.remaining"), "3");
    EXPECT_EQ(lua("local same = m.shuffleBag({'a', 'b', 'c'}, {seed = 9}) local again = m.shuffleBag({'a', 'b', 'c'}, {seed = 9}) return same:next() == again:next()"), "true");
    EXPECT_NE(lua("return m.shuffleBag({'a', 'b'}, {counts = {1}})").find("one count per item"), std::string::npos);
    EXPECT_NE(lua("return m.shuffleBag({'a'}, {counts = {0}})").find("at least one item"), std::string::npos);

    // clang-format off
    fixture.runLua(R"(
        loot = m.weightedChoice({'common', 'rare', 'never'}, {9, 1, 0}, 3)
        counts = {common = 0, rare = 0, never = 0}
        for i = 1, 2000 do
            local item = loot:pick()
            counts[item] = counts[item] + 1
        end
    )");
    // clang-format on
    EXPECT_EQ(lua("return tostring(counts.common > counts.rare * 5) .. ' ' .. counts.never .. ' ' .. loot.size .. ' ' .. string.format('%.3f', loot:probability(2))"), "true 0 3 0.100");
    EXPECT_EQ(lua("return loot:pick(m.random(5)) ~= nil"), "true");
    EXPECT_NE(lua("return m.weightedChoice({'a'}, {1, 2})").find("one weight per item"), std::string::npos);
    EXPECT_NE(lua("return m.weightedChoice({'a'}, {-1})").find("not negative"), std::string::npos);
}

TEST_F(GeometryLuaTest, WorleyWarpAndVaryingPoisson) {
    fixture.runLua("noise = m.noise(3) nearest, second, cell = noise:worley(1.5, 2.5) wx, wy = noise:warp(10, 20, 4, 0.1)");
    EXPECT_EQ(lua("return tostring(nearest <= second) .. ' ' .. tostring(cell >= 0 and cell < 1) .. ' ' .. tostring(math.abs(wx - 10) <= 4 and math.abs(wy - 20) <= 4)"), "true true true");

    // clang-format off
    fixture.runLua(R"(
        points = m.poissonDisk({area = {0, 0, 200, 100}, minimumDistance = 5, maximumDistance = 20, seed = 2, distance = function(point) return point.x < 100 and 5 or 20 end})
        left = 0
        for _, point in ipairs(points) do
            if point.x < 100 then left = left + 1 end
        end
    )");
    // clang-format on
    EXPECT_EQ(lua("return tostring(left > (#points - left) * 4)"), "true");
    EXPECT_NE(lua("return m.poissonDisk({area = {0, 0, 10, 10}, minimumDistance = 5, maximumDistance = 1, distance = function() return 5 end})").find("maximum distance"), std::string::npos);
}

} // namespace haylen
