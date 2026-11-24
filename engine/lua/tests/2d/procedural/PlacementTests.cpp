#include <gtest/gtest.h>

#include <chrono>
#include <string>

#include "support/LuaFixture.hpp"

namespace haylen::lua::procedural2d {

// Lua places kinds counted from 1 with shape tables, returns the entities and what each kind left out, and does the same work on a worker thread.
TEST(PlacementLuaTest, PlacesKindsFromLua) {
    test::LuaFixture fixture;
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

} // namespace haylen::lua::procedural2d
