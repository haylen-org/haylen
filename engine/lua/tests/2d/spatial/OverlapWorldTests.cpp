#include <gtest/gtest.h>

#include <string>

#include "support/LuaFixture.hpp"

namespace haylen::lua::spatial2d {

TEST(OverlapWorldLuaTest, PlacesFromBuffersAndWritesContactsIntoThem) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        spatial2d = require('haylen.spatial2d')
        collections = require('haylen.collections')
        world = spatial2d.newOverlapWorld({kinds = {'body', 'hit'}, cellSize = 128})
        standing = world:defineSet({{kind = 'body', x = -18, y = -78, width = 36, height = 78, layers = 1, depth = 12}})
        punching = world:defineSet({{kind = 'body', x = -18, y = -78, width = 36, height = 78, layers = 1, depth = 12}, {kind = 'hit', x = 12, y = -62, width = 34, height = 14, mask = 1}})
        hero, rival = world:add(), world:add()
        placements = collections.newFloatBuffer(12)
        placements:set(1, {hero, 200, 0, 420, 1, punching, rival, 236, 0, 426, -1, standing})
        world:placeAll(placements)
        contacts = collections.newFloatBuffer(8)
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return world:contacts(contacts) .. ' ' .. contacts[1] .. ' ' .. contacts[2] .. ' ' .. contacts[3] .. ' ' .. contacts[7]"), "1 1.0 2.0 2.0 28.0");
    EXPECT_EQ(fixture.lua("local list = world:contacts() return #list .. ' ' .. list[1].target .. ' ' .. list[1].overlap.width"), "1 2 28.0");
    EXPECT_EQ(fixture.lua("world:rest(hero, rival, 1) local resting = world:resting(hero, rival) world:step() return tostring(resting) .. ' ' .. tostring(world:resting(hero, rival)) .. ' ' .. world.entityCount"), "true false 2");
    EXPECT_EQ(fixture.lua("world:place(rival, 236, 0, 426, -1, 0) return world:contacts(contacts)"), "0");

    fixture.runLua("local graphics2d = require('haylen.graphics2d') world:place(rival, 236, 0, 426, -1, standing) graphics2d.beginWorld(graphics2d.newCamera()) world:debugDraw({layer = 10})");
    EXPECT_NE(fixture.lua("world:defineSet({{kind = 'kick', width = 1, height = 1}})").find("The box kind \"kick\" is not one of the kinds of the overlap world."), std::string::npos);
    EXPECT_NE(fixture.lua("world:defineSet({{width = 0, height = 1}})").find("A box needs a positive width and height."), std::string::npos);
    EXPECT_NE(fixture.lua("world:place(9, 0, 0, 0, 1, 1)").find("The overlap world has no entity 9."), std::string::npos);
    EXPECT_NE(fixture.lua("world:place(hero, 0, 0, 0, 1, 4)").find("The overlap world has no box set 4."), std::string::npos);
    EXPECT_NE(fixture.lua("world:defineSet({{width = 1, height = 1, mask = -1}})").find("The \"mask\" of a box is an integer of 32 bits."), std::string::npos);
    EXPECT_NE(fixture.lua("spatial2d.newOverlapWorld({cells = 1})").find("Unknown option \"cells\"."), std::string::npos);
    EXPECT_NE(fixture.lua("spatial2d.newOverlapWorld({cellSize = -1})").find("The cell size of an overlap world must be 0 or positive."), std::string::npos);
}

// An entity or a set of a buffer that is no whole number of 32 bits names nothing, so it fails instead of placing whatever its conversion lands on.
TEST(OverlapWorldLuaTest, RejectsPlacementsWhoseIdsAreNotWholeNumbersOf32Bits) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        spatial2d = require('haylen.spatial2d')
        collections = require('haylen.collections')
        world = spatial2d.newOverlapWorld()
        set = world:defineSet({{width = 10, height = 10}})
        entity = world:add()
        placements = collections.newFloatBuffer(6)
    )");
    // clang-format on
    for (const std::string& group : {"entity + 0.5, 4, 0, 0, 1, set", "-1, 4, 0, 0, 1, set", "5e9, 4, 0, 0, 1, set", "0 / 0, 4, 0, 0, 1, set", "entity, 4, 0, 0, 1, set + 0.5"}) {
        EXPECT_NE(fixture.lua("placements:set(1, {" + group + "}) world:placeAll(placements)").find("The entity and the set of a placement in a buffer are whole numbers of 32 bits."), std::string::npos) << group;
    }

    fixture.runLua("placements:set(1, {entity, 4, 0, 0, 1, set}) world:placeAll(placements)");
}

TEST(OverlapWorldLuaTest, NamesTheBoxOfASetThatIsNotATable) {
    test::LuaFixture fixture;
    EXPECT_NE(fixture.lua("require('haylen.spatial2d').newOverlapWorld({kinds = {'body'}}):defineSet({1})").find("The item 1 of \"boxes\" of \"defineSet\" is invalid: table expected, got number."), std::string::npos);
}

} // namespace haylen::lua::spatial2d
