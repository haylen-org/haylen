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

TEST(Physics3DLuaTest, MakesShapesInMetersWithTheirVolumeAndMass) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        physics3d = require('haylen.physics3d')
        math3d = require('haylen.math')
        ball = physics3d.newSphere(0.5)
        crate = physics3d.newBox({2, 1, 0.5}, {density = 500, convexRadius = 0.02})
        pill = physics3d.newCapsule(2, 0.25)
        cone = physics3d.newTaperedCylinder(1, 0.1, 0.5)
        hull = physics3d.newConvexHull({{-0.5, -0.5, -0.5}, {0.5, -0.5, -0.5}, {-0.5, 0.5, -0.5}, {0.5, 0.5, -0.5}, {-0.5, -0.5, 0.5}, {0.5, -0.5, 0.5}, {-0.5, 0.5, 0.5}, {0.5, 0.5, 0.5}}, {convexRadius = 0})
        pair = physics3d.newCompound({{shape = crate, position = {0, 2, 0}}, {shape = ball, position = {0, -2, 0}, rotation = math3d.quat()}})
        tall = physics3d.newScaled(crate, {1, 2, 1})
        low = physics3d.newOffsetCenterOfMass(crate, {0, -0.4, 0})
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("return ball.kind .. ' ' .. crate.kind .. ' ' .. pill.kind .. ' ' .. cone.kind .. ' ' .. hull.kind .. ' ' .. pair.kind .. ' ' .. tall.kind .. ' ' .. low.kind"), "sphere box capsule taperedCylinder convexHull staticCompound scaled offsetCenterOfMass");
    EXPECT_EQ(fixture.lua("return string.format('%.3f %.1f %.2f %.2f', ball.volume, crate.mass, crate.bounds.max.x, pill.bounds.max.y)"), "0.524 500.0 1.00 1.00");
    EXPECT_EQ(fixture.lua("return string.format('%.3f %.1f %.2f %s %s', hull.volume, tall.mass, low.centerOfMass.y, tostring(pair.solid), ball.backend)"), "1.000 1000.0 -0.40 true jolt");
    EXPECT_EQ(fixture.lua("return tostring(ball == ball) .. ' ' .. tostring(ball == crate)"), "true false");
}

TEST(Physics3DLuaTest, MakesSurfacesFromBuffersMeshTreesAndHeights) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        physics3d = require('haylen.physics3d')
        spatial3d = require('haylen.spatial3d')
        collections = require('haylen.collections')
        floor = {{-5, 0, -5}, {5, 0, -5}, {5, 0, 5}, {-5, 0, 5}}
        ground = physics3d.newMesh(floor, {1, 3, 2, 1, 4, 3})
        tree = physics3d.newMesh(spatial3d.newMeshBvh(floor, {1, 3, 2, 1, 4, 3}))
        heights = collections.newFloatBuffer(16)
        heights[6] = 3
        hills = physics3d.newHeightField(heights, 4, {scale = {2, 1, 2}, holes = {16}})
        sea = physics3d.newPlane({0, 1, 0}, -1, {halfExtent = 100})
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("return ground.kind .. ' ' .. tree.kind .. ' ' .. hills.kind .. ' ' .. sea.kind .. ' ' .. tostring(ground.solid) .. ' ' .. ground.mass"), "mesh mesh heightField plane false 0.0");
    EXPECT_EQ(fixture.lua("return string.format('%.1f %.1f %.1f', tree.bounds.min.z, hills.bounds.max.x, hills.bounds.max.y)"), "-5.0 6.0 3.0");
}

TEST(Physics3DLuaTest, BuildsHeavyShapesOnTheTaskPool) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        physics3d = require('haylen.physics3d')
        async = require('async')
        async.spawn(function()
            local mesh = physics3d.newMeshAsync({{0, 0, 0}, {1, 0, 0}, {0, 0, 1}}):await()
            local hull = physics3d.newConvexHullAsync({{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}}):await()
            local field = physics3d.newHeightFieldAsync({0, 0, 0, 0, 1, 0, 0, 0, 0}, 3):await()
            local _, failure = physics3d.newConvexHullAsync({{0, 0, 0}, {1, 0, 0}, {0, 0, 1}, {1, 0, 1}}):await()
            done = mesh.kind .. ' ' .. hull.kind .. ' ' .. field.kind .. ' ' .. tostring(failure)
        end)
    )");
    // clang-format on

    ASSERT_TRUE(fixture.frameUntil([&fixture] { return fixture.lua("return done") != "nil" || fixture.engine().getError() != nullptr; }));
    ASSERT_EQ(fixture.engine().getError(), nullptr) << fixture.engine().getError()->what();
    EXPECT_EQ(fixture.lua("return done"), "mesh convexHull heightField The points of a convex hull lie on one plane or one line, so they hold no volume. Give it points that spread along every axis.");
}

TEST(Physics3DLuaTest, RefusesShapesWithSentencesThatSayWhatTheyTake) {
    test::LuaFixture fixture;
    fixture.runLua("physics3d = require('haylen.physics3d')");

    EXPECT_EQ(fixture.lua("local ok, message = pcall(physics3d.newSphere, -1) return message:match('The radius.*')"), "The radius of a sphere must be a finite number above zero.");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(physics3d.newSphere, 1, {convexRadius = 0.1}) return message:match('Unknown.*')"), "Unknown option \"convexRadius\".");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(physics3d.newMesh, {{0, 0, 0}, {1, 0, 0}, {0, 0, 1}}, {1, 2, 4}) return message:match('Item.*')"), "Item 3 of the indices of a mesh names vertex 4, but the vertex count of the mesh is 3.");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(physics3d.newHeightField, {0, 0, 0}, 3) return message:match('A heightfield.*')"), "A heightfield of 3 samples on each side needs 9 heights, and it has 3.");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(physics3d.newHeightField, {0, 0, 0, 0, 0, 0, 0, 0, 0}, 3, {holes = {10}}) return message:match('Item.*')"), "Item 1 of the holes of a heightfield must name a sample from 1 to 9.");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(physics3d.newCompound, {}) return message:match('A static.*')"), "A static compound needs at least one child.");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(physics3d.newScaled, physics3d.newSphere(1), {1, 2, 1}) return message:match('The scale.*')"), "The scale does not suit the shape it scales. Spheres and capsules take the same scale on every axis, cylinders the same scale on x and z, and the children of a compound the scales their own shapes take.");
}

} // namespace haylen::lua::physics3d
