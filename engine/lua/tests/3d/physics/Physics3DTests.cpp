#include <gtest/gtest.h>

#include <string>

#include "support/LuaFixture.hpp"

namespace haylen::lua::physics3d {

TEST(Physics3DLuaTest, MakesAndStepsWorldsInMeters) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        physics3d = require('haylen.physics3d')
        math3d = require('haylen.math')
        world = physics3d.newWorld()
        for i = 1, 30 do world:step(1 / 60) end
        stats = world:stats()
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("return world.backend .. ' ' .. table.concat(physics3d.backends(), ',')"), "jolt jolt");
    EXPECT_EQ(fixture.lua("return tostring(world.gravity == math3d.vec3(0, -math3d.standardGravity, 0))"), "true");
    EXPECT_EQ(fixture.lua("return world.velocitySteps .. ' ' .. world.positionSteps .. ' ' .. world.collisionSteps .. ' ' .. tostring(world.sleepEnabled)"), "10 2 1 true");
    EXPECT_EQ(fixture.lua("return world.maxBodies .. ' ' .. world.maxBodyPairs .. ' ' .. world.maxContactConstraints .. ' ' .. world.temporaryMemory"), "10240 65536 20480 16");
    EXPECT_EQ(fixture.lua("return stats.bodies .. ' ' .. stats.activeBodies .. ' ' .. world.bodyCount .. ' ' .. type(stats.stepMilliseconds) .. ' ' .. math.type(world:stateHash())"), "0 0 0 number integer");
}

TEST(Physics3DLuaTest, ChangesTheSettingsOfAWorldAndRefusesValuesOutOfRange) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        physics3d = require('haylen.physics3d')
        world = physics3d.newWorld({gravity = {0, -1.62, 0}, velocitySteps = 6, sleepEnabled = false, maxBodies = 512, threads = 1})
        world.positionSteps = 3
        world.restitutionThreshold = 0.5
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("return string.format('%.2f %d %d %s %d %d %.1f', world.gravity.y, world.velocitySteps, world.positionSteps, tostring(world.sleepEnabled), world.maxBodies, world.threads, world.restitutionThreshold)"), "-1.62 6 3 false 512 1 0.5");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(function() world.velocitySteps = 1 end) return message:match('The value.*')"), "The value of \"velocitySteps\" must be from 2 to 64.");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(physics3d.newWorld, {maxBodies = 0}) return message:match('The value.*')"), "The value of \"maxBodies\" must be from 1 to 8388607.");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(physics3d.newWorld, {velocity = 8}) return message:match('Unknown.*')"), "Unknown option \"velocity\".");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(function() world:step(-1) end) return message:match('A step.*')"), "A step of a 3D physics world takes a time of zero seconds or more.");
}

TEST(Physics3DLuaTest, EndsAWorldWithItsOwnerOrWhenDestroyed) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        physics3d = require('haylen.physics3d')
        owner = {}
        owned = physics3d.newWorld({owner = owner})
        kept = physics3d.newWorld()
        kept:destroy()
        owner = nil
        collectgarbage()
        collectgarbage()
    )");
    // clang-format on

    // What a collected owner held ends at the end of the frame.
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return tostring(owned.destroyed) .. ' ' .. tostring(kept.destroyed)"), "true true");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(function() kept:step(1 / 60) end) return message:match('The 3D.*')"), "The 3D physics world was destroyed.");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(function() return owned.gravity end) return message:match('The 3D.*')"), "The 3D physics world was destroyed.");
}

TEST(Physics3DLuaTest, StartsEveryWorldFromThePhysics3DSectionOfHaylenJson) {
    test::LuaFixture fixture({{"haylen.json", R"({"name": "Moon Base", "identifier": "com.haylen.moon", "physics3d": {"gravity": [0, -1.62, 0], "velocitySteps": 8, "temporaryMemory": 4}})"}});
    EXPECT_EQ(fixture.lua("local world = require('haylen.physics3d').newWorld({positionSteps = 1}) return string.format('%.2f %d %d %d', world.gravity.y, world.velocitySteps, world.positionSteps, world.temporaryMemory)"), "-1.62 8 1 4");
}

} // namespace haylen::lua::physics3d
