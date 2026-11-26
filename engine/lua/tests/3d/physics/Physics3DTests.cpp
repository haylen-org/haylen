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

TEST(Physics3DLuaTest, MakesBodiesThatFallRestAndKeepTheirData) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        physics3d = require('haylen.physics3d')
        world = physics3d.newWorld()
        floor = world:createBody({type = 'static', shape = physics3d.newBox({100, 1, 100}), position = {0, -0.5, 0}})
        ball = world:createBody({shape = physics3d.newSphere(0.25), position = {0, 3, 0}, friction = 0.4, restitutionCombine = 'maximum'})
        ball.data = {name = 'marble'}
        for i = 1, 240 do world:step(1 / 60) end
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("return string.format('%.1f %s %s %s %.1f', ball.y, tostring(ball.awake), ball.type, ball.restitutionCombine, ball.friction)"), "0.2 false dynamic maximum 0.4");
    EXPECT_EQ(fixture.lua("return ball.data.name .. ' ' .. tostring(ball.world == world) .. ' ' .. tostring(ball == ball) .. ' ' .. tostring(ball == floor) .. ' ' .. ball.shape.kind .. ' ' .. world.bodyCount"), "marble true true false sphere 2");
    EXPECT_EQ(fixture.lua("return string.format('%.1f %s %s', ball.mass, ball.motionQuality, tostring(ball.locks.y))"), "65.4 discrete false");
}

TEST(Physics3DLuaTest, PushesMovesAndTeleportsBodies) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        physics3d = require('haylen.physics3d')
        m = require('haylen.math')
        world = physics3d.newWorld({gravity = {0, 0, 0}})
        crate = world:createBody({shape = physics3d.newBox({1, 1, 1}), mass = 2, linearDamping = 0, angularDamping = 0})
        crate:applyImpulse({4, 0, 0})
        crate:applyImpulse({0, 0, 1}, {0.5, 0, 0})
        speed = crate.linearVelocity.x
        spin = crate.angularVelocity.y
        crate:setTransform({0, 9, 0}, m.fromAxisAngle({0, 1, 0}, 1))
        turned = crate.rotation.y
        platform = world:createBody({type = 'kinematic', shape = physics3d.newBox({4, 0.5, 4})})
        platform:moveTo({1, 0, 0}, m.quat(), 0.5)
        world:step(1 / 60)
        crate.locks = {x = true, rotationY = true}
        crate.z = 3
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("return string.format('%.1f %s %.1f %.2f %.1f', speed, tostring(spin < 0), crate.y, turned, platform.linearVelocity.x)"), "2.0 true 9.0 0.48 2.0");
    EXPECT_EQ(fixture.lua("return string.format('%s %s %.1f %.1f', tostring(crate.locks.x), tostring(crate.locks.rotationY), crate.position.z, crate:velocityAt(crate.worldCenter).x)"), "true true 3.0 0.0");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(function() platform:applyForce({1, 0, 0}) end) return message:match('Only.*')"), "Only a dynamic 3D body takes forces.");
}

TEST(Physics3DLuaTest, TweensTheTransformOfABodyNatively) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        physics3d = require('haylen.physics3d')
        tween = require('haylen.tween')
        m = require('haylen.math')
        world = physics3d.newWorld({gravity = {0, 0, 0}})
        door = world:createBody({type = 'kinematic', shape = physics3d.newBox({1, 2, 0.1})})
        tween.to(door, 1, {position = m.vec3(4, 0, 0), rotation = m.fromAxisAngle({0, 1, 0}, math.pi / 2)})
        gone = world:createBody({type = 'kinematic', shape = physics3d.newSphere(1)})
        tween.to(gone, 1, {position = m.vec3(0, 5, 0)})
        gone:destroy()
    )");
    // clang-format on

    fixture.frames(2, 0.25);
    EXPECT_EQ(fixture.lua("return string.format('%.1f %.3f', door.x, door.rotation:angle())"), "2.0 0.785");
    fixture.frames(2, 0.25);
    ASSERT_EQ(fixture.engine().getError(), nullptr) << fixture.engine().getError()->what();
    EXPECT_EQ(fixture.lua("return string.format('%.1f %s', door.x, tostring(gone.valid))"), "4.0 false");
}

TEST(Physics3DLuaTest, ReadsAndWritesTheTransformsOfManyBodiesThroughABuffer) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        physics3d = require('haylen.physics3d')
        collections = require('haylen.collections')
        world = physics3d.newWorld({gravity = {0, 0, 0}, interpolate = true})
        ball = physics3d.newSphere(0.5)
        bodies = {}
        for i = 1, 3 do bodies[i] = world:createBody({shape = ball, position = {i, 0, 0}, linearVelocity = {0, 6, 0}, linearDamping = 0}) end
        world:step(1 / 60)
        world:step(1 / 60)
        buffer = collections.newFloatBuffer(22)
        world:readTransforms(bodies, buffer, 2)
        smooth = collections.newFloatBuffer(21)
        world:readTransforms(bodies, smooth, 1, true)
        buffer[16] = 9
        world:writeTransforms(bodies, buffer, 2)
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("return string.format('%.1f %.1f %.2f %.1f %s', buffer[2], buffer[9], buffer[10], buffer[8], tostring(smooth[2] <= buffer[3]))"), "1.0 2.0 0.20 1.0 true");
    EXPECT_EQ(fixture.lua("return string.format('%.1f %s', bodies[3].x, tostring(world.interpolate))"), "9.0 true");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(function() world:readTransforms(bodies, collections.newFloatBuffer(20)) end) return message:match('Body.*')"), "Body transforms take seven floats for each body.");
    EXPECT_EQ(fixture.lua("local x, rotation = bodies[1]:renderTransform() return string.format('%.1f %.1f', x.x, rotation.w)"), "1.0 1.0");
}

TEST(Physics3DLuaTest, ChangesTheChildrenOfTheOwnCompoundOfABody) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        physics3d = require('haylen.physics3d')
        world = physics3d.newWorld({gravity = {0, 0, 0}})
        crate = physics3d.newBox({1, 1, 1})
        ship = physics3d.newCompound({{shape = crate}}, {mutable = true})
        first = world:createBody({shape = ship})
        second = world:createBody({shape = ship, position = {10, 0, 0}})
        index = first:addChild(crate, {2, 0, 0})
        first:setChildTransform(2, {0, 2, 0}, require('haylen.math').quat())
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("return index .. ' ' .. first.childCount .. ' ' .. second.childCount .. ' ' .. string.format('%.1f', first.bounds.max.y)"), "2 2 1 2.5");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(function() first:removeChild(3) end) return message:match('The 3D.*')"), "The 3D body has no child 3, since its children go from 1 to 2.");
    EXPECT_EQ(fixture.lua("first:removeChild(1) return first.childCount"), "1");
}

TEST(Physics3DLuaTest, RefusesBodiesThatTheirOptionsDoNotSuitAndHandlesThatOutliveThem) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        physics3d = require('haylen.physics3d')
        world = physics3d.newWorld()
        ball = physics3d.newSphere(0.5)
        body = world:createBody({shape = ball})
        body.data = 'kept'
        body:destroy()
        other = physics3d.newWorld()
        stranger = other:createBody({shape = ball})
        other:destroy()
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("local ok, message = pcall(function() world:createBody({}) end) return message:match('A 3D.*')"), "A 3D body needs the option \"shape\", such as \"physics3d.newBox({1, 1, 1})\".");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(function() world:createBody({shape = ball, spin = 1}) end) return message:match('Unknown.*')"), "Unknown option \"spin\".");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(function() world:createBody({shape = ball, type = 'flying'}) end) return tostring(message:find('flying') ~= nil)"), "true");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(function() world:createBody({shape = physics3d.newPlane({0, 1, 0}, 0)}) end) return message:match('A dynamic.*')"), "A dynamic 3D body needs a shape with a volume, and a plane has none. Make the body static or kinematic, or give it a convex hull or a compound of convex shapes.");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(function() return body.position end) return tostring(body.valid) .. ' ' .. message:match('The 3D.*')"), "false The 3D physics body was destroyed.");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(function() return stranger.mass end) return message:match('The 3D.*')"), "The 3D physics world of the body was destroyed.");
}

TEST(Physics3DLuaTest, FiltersCollisionsByTheCategoriesOfHaylenJson) {
    test::LuaFixture fixture({{"haylen.json", R"({"name": "Layers", "identifier": "com.haylen.layers", "physics3d": {"categories": ["ground", "player", "ghost"]}})"}});
    // clang-format off
    fixture.runLua(R"(
        physics3d = require('haylen.physics3d')
        world = physics3d.newWorld()
        crate = physics3d.newBox({1, 1, 1})
        world:createBody({type = 'static', shape = physics3d.newBox({40, 1, 40}), position = {0, -0.5, 0}, category = 'ground'})
        hero = world:createBody({shape = crate, position = {-3, 1, 0}, category = 'player', mask = {'ground', 'player'}})
        ghost = world:createBody({shape = crate, position = {3, 1, 0}, category = 'ghost', mask = 'player'})
        for i = 1, 90 do world:step(1 / 60) end
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("return string.format('%s %s %d %d %d', tostring(hero.y > 0.4), tostring(ghost.y < -1), hero.category, hero.mask, physics3d.category('ghost'))"), "true true 2 3 4");
    EXPECT_EQ(fixture.lua("return table.concat(physics3d.categories(), ',')"), "ground,player,ghost");
    EXPECT_EQ(fixture.lua("hero.group = -2 hero.mask = 0xFFFFFFFF return hero.group .. ' ' .. hero.mask"), "-2 4294967295");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(function() hero.category = 'water' end) return message:match('No 3D.*')"), "No 3D collision category is named \"water\". The categories of \"physics3d.categories\" in \"haylen.json\" are \"ground\", \"player\", \"ghost\".");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(function() hero.mask = -1 end) return message:match('The option.*')"), "The option \"mask\" of a 3D body takes the names of categories or whole numbers of 32 bits.");
}

} // namespace haylen::lua::physics3d
