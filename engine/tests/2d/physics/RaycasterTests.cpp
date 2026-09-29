#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/physics/RayBatch.hpp"
#include "haylen/2d/physics/RayDebugDraw.hpp"
#include "haylen/2d/physics/Raycaster.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/core/SceneManager.hpp"
#include "support/DrawingScene.hpp"
#include "support/EngineFixture.hpp"

namespace haylen {

namespace {

// Builds three thin posts along x at 100, 200 and 300 and two mirrors at x = -400 and x = 400, with the given categories and groups.
struct Range {
    physics2d::World world{{.gravity = {}}};
    std::vector<physics2d::Shape> posts;
    physics2d::Shape leftMirror;
    physics2d::Shape rightMirror;

    Range() {
        for (int index = 0; index < 3; ++index) {
            physics2d::Body post = world.createBody({.type = physics2d::Body::Type::Static, .position = {100.0F * static_cast<float>(index + 1), 0.0F}});
            posts.push_back(post.addBox({10.0F, 100.0F}, {.filter = {.category = index == 1 ? 2U : 1U, .group = index == 2 ? -3 : 0}}));
        }
        leftMirror = world.createBody({.type = physics2d::Body::Type::Static, .position = {-400.0F, 0.0F}}).addBox({10.0F, 1000.0F}, {.filter = {.category = 4}});
        rightMirror = world.createBody({.type = physics2d::Body::Type::Static, .position = {400.0F, 0.0F}}).addBox({10.0F, 1000.0F}, {.filter = {.category = 4}});
    }
};

} // namespace

TEST(RaycasterTest, FindsTheClosestHitEveryHitAndFilteredHits) {
    Range range;
    const physics2d::Raycaster caster(range.world);

    const std::optional<physics2d::RaycastHit> closest = caster.castRay({0.0F, 0.0F}, {1000.0F, 0.0F});
    ASSERT_TRUE(closest.has_value());
    EXPECT_EQ(closest->shape, range.posts[0]);
    EXPECT_NEAR(closest->point.x, 95.0F, 0.1F);
    EXPECT_NEAR(closest->normal.x, -1.0F, 1e-4F);
    EXPECT_NEAR(closest->fraction, 0.095F, 1e-4F);
    EXPECT_NEAR(closest->distance, 95.0F, 0.1F);
    EXPECT_EQ(closest->shape.getBody().getPosition(), (math::Vec2{100.0F, 0.0F}));

    std::vector<physics2d::RaycastHit> hits;
    caster.castRayAll({0.0F, 0.0F}, {1000.0F, 0.0F}, {}, 0, hits);
    ASSERT_EQ(hits.size(), 4U);
    EXPECT_EQ(hits[1].shape, range.posts[1]);
    EXPECT_EQ(hits[3].shape, range.rightMirror);
    for (std::size_t index = 1; index < hits.size(); ++index) {
        EXPECT_LT(hits[index - 1].fraction, hits[index].fraction);
    }

    // A limit pierces a number of shapes, and masks, groups and accept functions pick what the ray sees.
    caster.castRayAll({0.0F, 0.0F}, {1000.0F, 0.0F}, {}, 2, hits);
    EXPECT_EQ(hits.size(), 2U);
    EXPECT_EQ(caster.castRay({0.0F, 0.0F}, {1000.0F, 0.0F}, {.collision = {.mask = 2}})->shape, range.posts[1]);
    EXPECT_EQ(caster.castRay({150.0F, 0.0F}, {1000.0F, 0.0F}, {.collision = {.mask = 4, .group = -3}})->shape, range.rightMirror);
    EXPECT_EQ(caster.castRay({150.0F, 0.0F}, {1000.0F, 0.0F}, {.collision = {.mask = 4, .group = 3}})->shape, range.rightMirror);
    EXPECT_EQ(caster.castRay({150.0F, 0.0F}, {1000.0F, 0.0F}, {.collision = {.mask = 2, .group = -3}})->shape, range.posts[1]);
    EXPECT_EQ(caster.castRay({250.0F, 0.0F}, {1000.0F, 0.0F}, {.collision = {.category = 0, .mask = 0, .group = -3}}), std::nullopt);
    physics2d::Shape ownPost = range.posts[2];
    ownPost.setFilter({.category = 1, .group = 3});
    EXPECT_EQ(caster.castRay({250.0F, 0.0F}, {1000.0F, 0.0F}, {.collision = {.mask = 4, .group = 3}})->shape, range.posts[2]);

    std::vector<float> offered;
    // clang-format off
    const physics2d::Raycaster::Filter picky{.accept = [&offered](const physics2d::RaycastHit& hit) {
        offered.push_back(hit.point.x);
        return hit.shape.getFilter().category == 4;
    }};
    // clang-format on
    EXPECT_EQ(caster.castRay({0.0F, 0.0F}, {1000.0F, 0.0F}, picky)->shape, range.rightMirror);
    EXPECT_EQ(offered.size(), 4U);
    caster.castRayAll({0.0F, 0.0F}, {1000.0F, 0.0F}, picky, 0, hits);
    EXPECT_EQ(hits.size(), 1U);

    EXPECT_TRUE(caster.hasLineOfSight({0.0F, 200.0F}, {300.0F, 200.0F}));
    EXPECT_FALSE(caster.hasLineOfSight({0.0F, 0.0F}, {300.0F, 0.0F}));
    EXPECT_TRUE(caster.hasLineOfSight({0.0F, 0.0F}, {300.0F, 0.0F}, {.collision = {.mask = 4}}));
    EXPECT_EQ(range.world.raycast({0.0F, 0.0F}, {1000.0F, 0.0F})->shape, range.posts[0]);
}

TEST(RaycasterTest, SweepsShapesUntilTheyTouch) {
    Range range;
    const physics2d::Raycaster caster(range.world);

    const std::optional<physics2d::RaycastHit> circle = caster.castCircle({0.0F, 0.0F}, 10.0F, {500.0F, 0.0F});
    ASSERT_TRUE(circle.has_value());
    EXPECT_EQ(circle->shape, range.posts[0]);
    EXPECT_NEAR(circle->distance, 85.0F, 0.5F);
    EXPECT_NEAR(circle->normal.x, -1.0F, 1e-3F);

    EXPECT_NEAR(caster.castBox({0.0F, 0.0F}, {40.0F, 20.0F}, 0.0F, {500.0F, 0.0F})->distance, 75.0F, 0.5F);
    EXPECT_NEAR(caster.castBox({0.0F, 0.0F}, {40.0F, 20.0F}, std::numbers::pi_v<float> * 0.5F, {500.0F, 0.0F})->distance, 85.0F, 0.5F);
    EXPECT_NEAR(caster.castCapsule({-20.0F, 0.0F}, {20.0F, 0.0F}, 5.0F, {500.0F, 0.0F})->distance, 70.0F, 0.5F);
    const std::vector<math::Vec2> wedge{{0.0F, -10.0F}, {30.0F, 0.0F}, {0.0F, 10.0F}};
    EXPECT_NEAR(caster.castPolygon(wedge, {500.0F, 0.0F})->distance, 65.0F, 0.5F);

    // Shapes that pass over or start inside behave as expected.
    EXPECT_FALSE(caster.castCircle({0.0F, 200.0F}, 10.0F, {300.0F, 0.0F}).has_value());
    const std::optional<physics2d::RaycastHit> inside = caster.castCircle({100.0F, 0.0F}, 2.0F, {10.0F, 0.0F});
    ASSERT_TRUE(inside.has_value());
    EXPECT_EQ(inside->fraction, 0.0F);
    EXPECT_TRUE(inside->normal.isZero());
    EXPECT_EQ(caster.castCircle({0.0F, 0.0F}, 10.0F, {500.0F, 0.0F}, {.collision = {.mask = 2}})->shape, range.posts[1]);

    EXPECT_THROW((void)caster.castCircle({}, -1.0F, {1.0F, 0.0F}), std::invalid_argument);
    EXPECT_THROW((void)caster.castBox({}, {0.0F, 1.0F}, 0.0F, {1.0F, 0.0F}), std::invalid_argument);
    EXPECT_THROW((void)caster.castCapsule({}, {1.0F, 0.0F}, 0.0F, {1.0F, 0.0F}), std::invalid_argument);
    EXPECT_THROW((void)caster.castPolygon(std::vector<math::Vec2>{{0.0F, 0.0F}, {1.0F, 0.0F}}, {1.0F, 0.0F}), std::invalid_argument);
}

TEST(RaycasterTest, BouncesPiercesAndFansRays) {
    Range range;
    const physics2d::Raycaster caster(range.world);
    const physics2d::Raycaster::Filter mirrors{.collision = {.mask = 4}};

    // A laser between the mirrors bounces until it runs out of bounces or of length.
    std::vector<physics2d::RaycastHit> hits;
    const math::Ray last = caster.bounce({{0.0F, 50.0F}, {1.0F, 0.0F}, 5000.0F}, 3, mirrors, hits);
    ASSERT_EQ(hits.size(), 4U);
    EXPECT_EQ(hits[0].shape, range.rightMirror);
    EXPECT_EQ(hits[1].shape, range.leftMirror);
    EXPECT_NEAR(hits[1].distance, 790.0F, 0.5F);
    EXPECT_NEAR(last.getEnd().x, -395.0F, 0.5F);
    const math::Ray open = caster.bounce({{0.0F, 50.0F}, {1.0F, 0.0F}, 1000.0F}, 5, mirrors, hits);
    EXPECT_EQ(hits.size(), 1U);
    EXPECT_NEAR(open.getEnd().x, -210.0F, 0.5F);
    EXPECT_THROW((void)caster.bounce({{0.0F, 0.0F}, {1.0F, 0.0F}}, 1, mirrors, hits), std::invalid_argument);

    std::vector<std::optional<physics2d::RaycastHit>> fan;
    caster.fan({0.0F, 0.0F}, 0.0F, std::numbers::pi_v<float>, 5, 1000.0F, mirrors, fan);
    ASSERT_EQ(fan.size(), 5U);
    EXPECT_FALSE(fan[0].has_value());
    EXPECT_EQ(fan[2]->shape, range.rightMirror);
    EXPECT_FALSE(fan[4].has_value());
    EXPECT_THROW(caster.fan({}, 0.0F, 1.0F, 3, std::numeric_limits<float>::infinity(), mirrors, fan), std::invalid_argument);
}

TEST(RaycasterTest, CastsBatchesInParallelLikeOneByOne) {
    test::EngineFixture fixture;
    Range range;
    const physics2d::Raycaster caster(range.world);
    physics2d::RayBatch batch;
    batch.resize(500);
    EXPECT_EQ(batch.size(), 500U);
    for (std::size_t index = 0; index < batch.size(); ++index) {
        const float y = static_cast<float>(index) - 250.0F;
        batch.setRay(index, {0.0F, y}, {1000.0F, y * 0.5F});
    }

    caster.castBatch(batch, {}, &fixture.engine().getJobs());
    std::size_t hits = 0;
    for (std::size_t index = 0; index < batch.size(); ++index) {
        const math::Segment& ray = batch.getRay(index);
        const std::optional<physics2d::RaycastHit> expected = caster.castRay(ray.start, ray.end);
        ASSERT_EQ(batch.getHit(index).has_value(), expected.has_value());
        if (expected) {
            EXPECT_EQ(batch.getHit(index)->shape, expected->shape);
            EXPECT_EQ(batch.getHit(index)->point, expected->point);
            ++hits;
        }
    }
    EXPECT_GT(hits, 100U);

    caster.castBatch(batch, {.mask = 4});
    EXPECT_EQ(batch.getHit(250)->shape, range.rightMirror);
    EXPECT_THROW((void)batch.getRay(500), std::out_of_range);
    EXPECT_THROW(batch.setRay(500, {}, {}), std::out_of_range);
}

TEST(RaycasterTest, DrawsRaysHitsAndNormalsForDebugging) {
    test::EngineFixture fixture;
    physics2d::RayDebugDraw rays;

    // The first ray of a frame drops the rays of earlier frames that were never drawn.
    rays.add(1, {0.0F, 0.0F}, {100.0F, 0.0F}, std::nullopt);
    rays.add(1, {0.0F, 0.0F}, {100.0F, 0.0F}, std::nullopt);
    rays.add(2, {0.0F, 0.0F}, {100.0F, 0.0F}, std::nullopt);
    rays.add(2, {0.0F, 10.0F}, {100.0F, 10.0F}, physics2d::RayDebugDraw::Hit{.point = {50.0F, 10.0F}, .normal = {-1.0F, 0.0F}});
    EXPECT_EQ(rays.size(), 2U);

    std::optional<physics2d::RayDebugDraw::Hit> mark;
    // clang-format off
    fixture.engine().getScenes().push(std::make_shared<test::DrawingScene>([&rays, &mark](core::Engine& engine) {
        engine.getRenderer2D().beginWorld(graphics2d::Camera{});
        rays.draw(engine.getRenderer2D(), {.layer = 10});
        physics2d::RayDebugDraw::drawRay(engine.getRenderer2D(), {0.0F, 20.0F}, {100.0F, 20.0F}, mark);
    }));
    // clang-format on

    // The collected rays draw once, and a hit adds its dot and normal to a plain ray.
    fixture.frames(1);
    const auto withCollected = fixture.engine().getRenderer2D().getStats().vertices;
    EXPECT_EQ(rays.size(), 0U);
    fixture.frames(1);
    const auto missOnly = fixture.engine().getRenderer2D().getStats().vertices;
    mark = physics2d::RayDebugDraw::Hit{.point = {60.0F, 20.0F}, .normal = {-1.0F, 0.0F}};
    fixture.frames(1);
    EXPECT_GT(withCollected, missOnly);
    EXPECT_GT(fixture.engine().getRenderer2D().getStats().vertices, missOnly);
}

TEST(WorldRaycastLuaTest, CastsRaysAndShapesFromLua) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        physics2d = require('haylen.physics2d')
        world = physics2d.newWorld({gravity = {0, 0}})
        posts = {}
        for index = 1, 3 do
            posts[index] = world:createBody({type = 'static', x = index * 100, y = 0})
            posts[index]:addBox(10, 100, {category = index == 2 and 2 or 1, group = index == 3 and -3 or 0})
            posts[index].data = {name = 'post' .. index}
        end
        mirror = world:createBody({type = 'static', x = 400, y = 0})
        mirror:addBox(10, 1000, {category = 4})
        mirror.data = {name = 'right'}
        leftMirror = world:createBody({type = 'static', x = -400, y = 0})
        leftMirror:addBox(10, 1000, {category = 4})
        leftMirror.data = {name = 'left'}
        function names(hits)
            local list = {}
            for _, hit in ipairs(hits) do list[#list + 1] = hit and hit.body.data.name or 'miss' end
            return table.concat(list, ',')
        end
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("local hit = world:raycast(0, 0, 1000, 0) return hit.body.data.name .. ' ' .. math.floor(hit.x + 0.5) .. ' ' .. hit.normalX .. ' ' .. math.floor(hit.distance + 0.5) .. ' ' .. tostring(hit.shape.valid)"), "post1 95 -1.0 95 true");
    EXPECT_EQ(fixture.lua("return names(world:raycastAll(0, 0, 1000, 0)) .. ' / ' .. names(world:raycastAll(0, 0, 1000, 0, {limit = 2}))"), "post1,post2,post3,right / post1,post2");
    EXPECT_EQ(fixture.lua("return world:raycast(0, 0, 1000, 0, {mask = 2}).body.data.name .. ' ' .. world:raycast(150, 0, 1000, 0, {mask = 5, group = -3}).body.data.name"), "post2 right");
    EXPECT_EQ(fixture.lua("return world:raycast(0, 0, 1000, 0, {accept = function(hit) return hit.body.data.name == 'post3' end}).body.data.name"), "post3");
    EXPECT_EQ(fixture.lua("return names(world:raycastAll(0, 0, 1000, 0, {accept = function(hit) return hit.x > 150 end, limit = 1}))"), "post2");
    EXPECT_EQ(fixture.lua("return tostring(world:lineOfSight(0, 200, 300, 200)) .. tostring(world:lineOfSight(0, 0, 300, 0)) .. tostring(world:lineOfSight(0, 0, 300, 0, {mask = 4}))"), "truefalsetrue");

    EXPECT_EQ(fixture.lua("return math.floor(world:castCircle(0, 0, 10, 500, 0).distance + 0.5) .. ' ' .. math.floor(world:castBox(0, 0, 40, 20, 0, 500, 0).distance + 0.5)"), "85 75");
    EXPECT_EQ(fixture.lua("return math.floor(world:castCapsule(-20, 0, 20, 0, 5, 500, 0).distance + 0.5) .. ' ' .. math.floor(world:castPolygon({{0, -10}, {30, 0}, {0, 10}}, 500, 0).distance + 0.5)"), "70 65");
    EXPECT_EQ(fixture.lua("return tostring(world:castCircle(0, 200, 10, 300, 0)) .. ' ' .. world:castCircle(0, 0, 10, 500, 0, {mask = 2}).body.data.name"), "nil post2");

    EXPECT_EQ(fixture.lua("local hits, x, y = world:bounceRay(0, 50, 1, 0, 5000, 2, {mask = 4}) return names(hits) .. ' ' .. math.floor(hits[2].distance + 0.5) .. ' ' .. math.floor(x + 0.5)"), "right,left,right 1185 395");
    EXPECT_EQ(fixture.lua("return names(world:rayFan(0, 0, 0, math.pi, 5, 1000, {mask = 4}))"), "miss,right,right,right,miss");
    EXPECT_NE(fixture.lua("world:rayFan(0, 0, 0, 1, 3, math.huge)").find("the length must be finite"), std::string::npos);

    // clang-format off
    fixture.runLua(R"(
        batch = physics2d.newRayBatch(3)
        batch:setRay(1, 0, 0, 1000, 0)
        batch:setRay(2, 0, 300, 1000, 300)
        batch:setRay(3, 0, 0, -1000, 0)
        world:raycastBatch(batch, {mask = 5})
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("local hit, x, y, nx, ny, fraction = batch:hit(1) return tostring(hit) .. ' ' .. math.floor(x + 0.5) .. ' ' .. nx .. ' ' .. string.format('%.3f', fraction) .. ' ' .. tostring(batch:hit(3)) .. ' ' .. batch.size"), "true 95 -1.0 0.095 true 3");
    EXPECT_EQ(fixture.lua("return batch:body(1).data.name .. ' ' .. batch:body(2).data.name .. ' ' .. tostring(batch:shape(1).valid) .. ' ' .. select(3, batch:ray(2))"), "post1 right true 1000.0");
    EXPECT_EQ(fixture.lua("batch.size = 1 return batch.size"), "1");
    EXPECT_EQ(fixture.lua("local empty = physics2d.newRayBatch() local before = empty.size empty.size = 2 return before .. ' ' .. empty.size .. ' ' .. select(3, empty:ray(2)) .. ' ' .. tostring(empty:hit(2))"), "0 2 0.0 false");

    fixture.runLua("world.debugRays = true world:raycast(0, 0, 1000, 0) world:raycastAll(0, 20, 1000, 20)");
    EXPECT_EQ(fixture.lua("return tostring(world.debugRays)"), "true");
    EXPECT_EQ(fixture.lua("world.debugRays = false return tostring(world.debugRays)"), "false");

    EXPECT_NE(fixture.lua("world:raycastBatch(batch, {accept = function() return true end})").find("no accept function"), std::string::npos);
    EXPECT_NE(fixture.lua("world:raycast(0, 0, 1, 0, {limit = 2})").find("Unknown option 'limit'"), std::string::npos);
    EXPECT_NE(fixture.lua("world:bounceRay(0, 0, 0, 0, 10, 1)").find("the direction must not be zero"), std::string::npos);
    EXPECT_NE(fixture.lua("world:raycast(0, 0, 1000, 0, {accept = function() error('filter failed') end})").find("filter failed"), std::string::npos);
    EXPECT_NE(fixture.lua("batch:hit(9)").find("outside the batch"), std::string::npos);

    // Recorded casts and single rays draw inside a world canvas, and screen points pick the shapes under them.
    // clang-format off
    fixture.runLua(R"(
        local graphics2d = require('haylen.graphics2d')
        camera = graphics2d.newCamera()
        local screen = require('haylen.viewport').visibleRect()
        cx, cy = screen.x + screen.width / 2, screen.y + screen.height / 2
        world.debugRays = true
        require('haylen.scene').push({
            update = function() world:rayFan(0, 0, 0, 1, 5, 500) end,
            render = function()
                graphics2d.beginWorld(camera)
                world:debugDrawRays({layer = 5})
                physics2d.drawRay(0, 0, 100, 0, world:raycast(0, 0, 1000, 0))
                physics2d.drawRay(0, 0, 100, 0, false, {layer = 6})
            end,
        })
    )");
    // clang-format on
    fixture.frames(2);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_GT(fixture.engine().getRenderer2D().getStats().vertices, 0U);

    EXPECT_EQ(fixture.lua("camera.position = {200, 0} local picked = world:pick(camera, cx, cy) return #picked .. ' ' .. picked[1].body.data.name"), "1 post2");
    EXPECT_EQ(fixture.lua("return #world:pick(camera, cx, cy - 200) .. ' ' .. #world:pick(camera, cx, cy, {mask = 1})"), "0 0");
    EXPECT_NE(fixture.lua("world:pick(camera, cx, cy, {group = 1})").find("Unknown option 'group'"), std::string::npos);
}

} // namespace haylen
