#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <vector>

#include "haylen/2d/physics/RayBatch.hpp"
#include "haylen/2d/physics/RayDebugDraw.hpp"
#include "haylen/2d/physics/Raycaster.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/core/SceneManager.hpp"
#include "support/DrawingScene.hpp"
#include "support/EngineFixture.hpp"
#include "support/Physics2DHelper.hpp"

namespace haylen::physics2d {

class RaycasterTest : public ::testing::Test {
  protected:
    // Builds three thin posts along x at 100, 200 and 300 and two mirrors at x = -400 and x = 400, with the given categories and groups.
    struct Range {
        World world{{.gravity = {}}};
        std::vector<Shape> posts;
        Shape leftMirror;
        Shape rightMirror;

        Range() {
            for (int index = 0; index < 3; ++index) {
                Body post = world.createBody({.type = Body::Type::Static, .position = {100.0F * static_cast<float>(index + 1), 0.0F}});
                posts.push_back(post.addBox({10.0F, 100.0F}, {.filter = {.category = index == 1 ? 2U : 1U, .group = index == 2 ? -3 : 0}}));
            }

            leftMirror = world.createBody({.type = Body::Type::Static, .position = {-400.0F, 0.0F}}).addBox({10.0F, 1000.0F}, {.filter = {.category = 4}});
            rightMirror = world.createBody({.type = Body::Type::Static, .position = {400.0F, 0.0F}}).addBox({10.0F, 1000.0F}, {.filter = {.category = 4}});
        }
    };
};

TEST_F(RaycasterTest, FindsTheClosestHitEveryHitAndFilteredHits) {
    Range range;
    const Raycaster caster(range.world);

    const std::optional<RaycastHit> closest = caster.castRay({0.0F, 0.0F}, {1000.0F, 0.0F});
    ASSERT_TRUE(closest.has_value());
    EXPECT_EQ(closest->shape, range.posts[0]);
    EXPECT_NEAR(closest->point.x, 95.0F, 0.1F);
    EXPECT_NEAR(closest->normal.x, -1.0F, 1e-4F);
    EXPECT_NEAR(closest->fraction, 0.095F, 1e-4F);
    EXPECT_NEAR(closest->distance, 95.0F, 0.1F);
    EXPECT_EQ(closest->shape.getBody().getPosition(), (haylen::math::Vec2{100.0F, 0.0F}));

    std::vector<RaycastHit> hits;
    caster.castRayAll({0.0F, 0.0F}, {1000.0F, 0.0F}, {}, 0, hits);
    ASSERT_EQ(hits.size(), 4U);
    EXPECT_EQ(hits[1].shape, range.posts[1]);
    EXPECT_EQ(hits[3].shape, range.rightMirror);

    for (std::size_t index = 1; index < hits.size(); ++index) {
        EXPECT_LT(hits[index - 1].fraction, hits[index].fraction);
    }

    // A limit pierces a number of shapes, and masks, groups and `accept` functions pick what the ray sees.
    caster.castRayAll({0.0F, 0.0F}, {1000.0F, 0.0F}, {}, 2, hits);
    EXPECT_EQ(hits.size(), 2U);
    EXPECT_EQ(test::Physics2DHelper::requireHit(caster.castRay({0.0F, 0.0F}, {1000.0F, 0.0F}, {.collision = {.mask = 2}})).shape, range.posts[1]);
    EXPECT_EQ(test::Physics2DHelper::requireHit(caster.castRay({150.0F, 0.0F}, {1000.0F, 0.0F}, {.collision = {.mask = 4, .group = -3}})).shape, range.rightMirror);
    EXPECT_EQ(test::Physics2DHelper::requireHit(caster.castRay({150.0F, 0.0F}, {1000.0F, 0.0F}, {.collision = {.mask = 4, .group = 3}})).shape, range.rightMirror);
    EXPECT_EQ(test::Physics2DHelper::requireHit(caster.castRay({150.0F, 0.0F}, {1000.0F, 0.0F}, {.collision = {.mask = 2, .group = -3}})).shape, range.posts[1]);
    EXPECT_EQ(caster.castRay({250.0F, 0.0F}, {1000.0F, 0.0F}, {.collision = {.category = 0, .mask = 0, .group = -3}}), std::nullopt);
    Shape ownPost = range.posts[2];
    ownPost.setFilter({.category = 1, .group = 3});
    EXPECT_EQ(test::Physics2DHelper::requireHit(caster.castRay({250.0F, 0.0F}, {1000.0F, 0.0F}, {.collision = {.mask = 4, .group = 3}})).shape, range.posts[2]);

    std::vector<float> offered;
    // clang-format off
    const Raycaster::Filter picky{.accept = [&offered](const RaycastHit& hit) {
        offered.push_back(hit.point.x);
        return hit.shape.getFilter().category == 4;
    }};
    // clang-format on
    EXPECT_EQ(test::Physics2DHelper::requireHit(caster.castRay({0.0F, 0.0F}, {1000.0F, 0.0F}, picky)).shape, range.rightMirror);
    EXPECT_EQ(offered.size(), 4U);
    caster.castRayAll({0.0F, 0.0F}, {1000.0F, 0.0F}, picky, 0, hits);
    EXPECT_EQ(hits.size(), 1U);

    EXPECT_TRUE(caster.hasLineOfSight({0.0F, 200.0F}, {300.0F, 200.0F}));
    EXPECT_FALSE(caster.hasLineOfSight({0.0F, 0.0F}, {300.0F, 0.0F}));
    EXPECT_TRUE(caster.hasLineOfSight({0.0F, 0.0F}, {300.0F, 0.0F}, {.collision = {.mask = 4}}));
    EXPECT_EQ(test::Physics2DHelper::requireHit(range.world.raycast({0.0F, 0.0F}, {1000.0F, 0.0F})).shape, range.posts[0]);
}

TEST_F(RaycasterTest, SweepsShapesUntilTheyTouch) {
    Range range;
    const Raycaster caster(range.world);

    const std::optional<RaycastHit> circle = caster.castCircle({0.0F, 0.0F}, 10.0F, {500.0F, 0.0F});
    ASSERT_TRUE(circle.has_value());
    EXPECT_EQ(circle->shape, range.posts[0]);
    EXPECT_NEAR(circle->distance, 85.0F, 0.5F);
    EXPECT_NEAR(circle->normal.x, -1.0F, 1e-3F);

    EXPECT_NEAR(test::Physics2DHelper::requireHit(caster.castBox({0.0F, 0.0F}, {40.0F, 20.0F}, 0.0F, {500.0F, 0.0F})).distance, 75.0F, 0.5F);
    EXPECT_NEAR(test::Physics2DHelper::requireHit(caster.castBox({0.0F, 0.0F}, {40.0F, 20.0F}, std::numbers::pi_v<float> * 0.5F, {500.0F, 0.0F})).distance, 85.0F, 0.5F);
    EXPECT_NEAR(test::Physics2DHelper::requireHit(caster.castCapsule({-20.0F, 0.0F}, {20.0F, 0.0F}, 5.0F, {500.0F, 0.0F})).distance, 70.0F, 0.5F);
    const std::vector<haylen::math::Vec2> wedge{{0.0F, -10.0F}, {30.0F, 0.0F}, {0.0F, 10.0F}};
    EXPECT_NEAR(test::Physics2DHelper::requireHit(caster.castPolygon(wedge, {500.0F, 0.0F})).distance, 65.0F, 0.5F);

    // Shapes that pass over or start inside behave as expected.
    EXPECT_FALSE(caster.castCircle({0.0F, 200.0F}, 10.0F, {300.0F, 0.0F}).has_value());
    const std::optional<RaycastHit> inside = caster.castCircle({100.0F, 0.0F}, 2.0F, {10.0F, 0.0F});
    ASSERT_TRUE(inside.has_value());
    EXPECT_EQ(inside->fraction, 0.0F);
    EXPECT_TRUE(inside->normal.isZero());
    EXPECT_EQ(test::Physics2DHelper::requireHit(caster.castCircle({0.0F, 0.0F}, 10.0F, {500.0F, 0.0F}, {.collision = {.mask = 2}})).shape, range.posts[1]);

    EXPECT_THROW((void)caster.castCircle({}, -1.0F, {1.0F, 0.0F}), std::invalid_argument);
    EXPECT_THROW((void)caster.castBox({}, {0.0F, 1.0F}, 0.0F, {1.0F, 0.0F}), std::invalid_argument);
    EXPECT_THROW((void)caster.castCapsule({}, {1.0F, 0.0F}, 0.0F, {1.0F, 0.0F}), std::invalid_argument);
    EXPECT_THROW((void)caster.castPolygon(std::vector<haylen::math::Vec2>{{0.0F, 0.0F}, {1.0F, 0.0F}}, {1.0F, 0.0F}), std::invalid_argument);
}

TEST_F(RaycasterTest, BouncesPiercesAndFansRays) {
    Range range;
    const Raycaster caster(range.world);
    const Raycaster::Filter mirrors{.collision = {.mask = 4}};

    // A laser between the mirrors bounces until it runs out of bounces or of length.
    std::vector<RaycastHit> hits;
    const haylen::math::Ray2 last = caster.bounce({{0.0F, 50.0F}, {1.0F, 0.0F}, 5000.0F}, 3, mirrors, hits);
    ASSERT_EQ(hits.size(), 4U);
    EXPECT_EQ(hits[0].shape, range.rightMirror);
    EXPECT_EQ(hits[1].shape, range.leftMirror);
    EXPECT_NEAR(hits[1].distance, 790.0F, 0.5F);
    EXPECT_NEAR(last.getEnd().x, -395.0F, 0.5F);
    const haylen::math::Ray2 open = caster.bounce({{0.0F, 50.0F}, {1.0F, 0.0F}, 1000.0F}, 5, mirrors, hits);
    EXPECT_EQ(hits.size(), 1U);
    EXPECT_NEAR(open.getEnd().x, -210.0F, 0.5F);
    EXPECT_THROW((void)caster.bounce({{0.0F, 0.0F}, {1.0F, 0.0F}}, 1, mirrors, hits), std::invalid_argument);

    std::vector<std::optional<RaycastHit>> fan;
    caster.fan({0.0F, 0.0F}, 0.0F, std::numbers::pi_v<float>, 5, 1000.0F, mirrors, fan);
    ASSERT_EQ(fan.size(), 5U);
    EXPECT_FALSE(fan[0].has_value());
    EXPECT_EQ(fan[2]->shape, range.rightMirror);
    EXPECT_FALSE(fan[4].has_value());
    EXPECT_THROW(caster.fan({}, 0.0F, 1.0F, 3, std::numeric_limits<float>::infinity(), mirrors, fan), std::invalid_argument);
}

TEST_F(RaycasterTest, CastsBatchesInParallelLikeOneByOne) {
    test::EngineFixture fixture;
    Range range;
    const Raycaster caster(range.world);
    RayBatch batch;
    batch.resize(500);
    EXPECT_EQ(batch.size(), 500U);

    for (std::size_t index = 0; index < batch.size(); ++index) {
        const float y = static_cast<float>(index) - 250.0F;
        batch.setRay(index, {0.0F, y}, {1000.0F, y * 0.5F});
    }

    caster.castBatch(batch, {}, &fixture.engine().getJobs());
    std::size_t hits = 0;
    for (std::size_t index = 0; index < batch.size(); ++index) {
        const haylen::math::Segment2& ray = batch.getRay(index);
        const std::optional<RaycastHit> expected = caster.castRay(ray.start, ray.end);
        ASSERT_EQ(batch.getHit(index).has_value(), expected.has_value());

        if (expected) {
            EXPECT_EQ(batch.getHit(index)->shape, expected->shape);
            EXPECT_EQ(batch.getHit(index)->point, expected->point);
            ++hits;
        }
    }

    EXPECT_GT(hits, 100U);

    caster.castBatch(batch, {.mask = 4});
    EXPECT_EQ(test::Physics2DHelper::requireHit(batch.getHit(250)).shape, range.rightMirror);
    EXPECT_THROW((void)batch.getRay(500), std::out_of_range);
    EXPECT_THROW(batch.setRay(500, {}, {}), std::out_of_range);
}

TEST_F(RaycasterTest, DrawsRaysHitsAndNormalsForDebugging) {
    test::EngineFixture fixture;
    RayDebugDraw rays;

    // The first ray of a frame drops the rays of earlier frames that were never drawn.
    rays.add(1, {0.0F, 0.0F}, {100.0F, 0.0F}, std::nullopt);
    rays.add(1, {0.0F, 0.0F}, {100.0F, 0.0F}, std::nullopt);
    rays.add(2, {0.0F, 0.0F}, {100.0F, 0.0F}, std::nullopt);
    rays.add(2, {0.0F, 10.0F}, {100.0F, 10.0F}, RayDebugDraw::Hit{.point = {50.0F, 10.0F}, .normal = {-1.0F, 0.0F}});
    EXPECT_EQ(rays.size(), 2U);

    std::optional<RayDebugDraw::Hit> mark;
    // clang-format off
    fixture.engine().getScenes().push(std::make_shared<test::DrawingScene>([&rays, &mark](haylen::core::Engine& engine) {
        engine.getRenderer2D().beginWorld(haylen::graphics2d::Camera{});
        rays.draw(engine.getRenderer2D(), {.layer = 10});
        RayDebugDraw::drawRay(engine.getRenderer2D(), {0.0F, 20.0F}, {100.0F, 20.0F}, mark);
    }));
    // clang-format on

    // The collected rays draw once, and a hit adds its dot and normal to a plain ray.
    fixture.frames(1);
    const auto withCollected = fixture.engine().getRenderer2D().getStats().instances;
    EXPECT_EQ(rays.size(), 0U);
    fixture.frames(1);
    const auto missOnly = fixture.engine().getRenderer2D().getStats().instances;
    mark = RayDebugDraw::Hit{.point = {60.0F, 20.0F}, .normal = {-1.0F, 0.0F}};
    fixture.frames(1);
    EXPECT_GT(withCollected, missOnly);
    EXPECT_GT(fixture.engine().getRenderer2D().getStats().instances, missOnly);
}

} // namespace haylen::physics2d
