#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "2d/lighting/ShadowMap.hpp"
#include "haylen/2d/lighting/Light.hpp"
#include "haylen/2d/lighting/LightFlicker.hpp"
#include "haylen/2d/lighting/Occluder.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/2d/tiled/MapRenderer.hpp"
#include "haylen/math/Math.hpp"

namespace haylen::lighting2d {

class ShadowMapTest : public ::testing::Test {
  protected:
    // Casts the shadow map row of a light over some occluders, the way the renderer does for a canvas that shows the bounds, and reads it the way the light shader does.
    struct CastRow {
        std::vector<float> row = std::vector<float>(ShadowMap::kRowTexels);
        ShadowMap::Axis axis{};

        CastRow(const Light& light, const std::vector<Occluder>& occluders, haylen::math::Rect bounds = {0.0F, 0.0F, 1000.0F, 1000.0F}) {
            std::vector<ShadowMap::Segment> segments;
            for (const Occluder& occluder : occluders) {
                ShadowMap::appendSegments(occluder, segments);
            }

            axis = ShadowMap::cast(light, segments, bounds, row);
        }

        // The shader finds the texel of the direction or of the place across the light, and a point deeper than the occluder depth there plus the bias is shadowed.
        [[nodiscard]] bool shadows(const Light& light, haylen::math::Vec2 point) const {
            const auto width = static_cast<float>(ShadowMap::kResolution);
            float coordinate = 0.0F;
            float depth = 0.0F;
            if (light.type == Light::Type::Directional) {
                const haylen::math::Vec2 direction = haylen::math::Vec2::fromAngle(light.rotation);
                coordinate = (haylen::math::Vec2::dot(point, direction.getPerpendicular()) - axis.acrossStart) / axis.acrossSpan * width;
                depth = (haylen::math::Vec2::dot(point, direction) - axis.alongStart) / axis.alongSpan;
            } else {
                const haylen::math::Vec2 offset = point - light.position;
                coordinate = (std::atan2(offset.y, offset.x) / haylen::math::Math::kTau + 0.5F) * width;
                depth = offset.getLength() / ShadowMap::getRange(light);
            }

            const auto size = ShadowMap::kResolution;
            const int texel = static_cast<int>(std::floor(coordinate));
            const int wrapped = light.type == Light::Type::Directional ? std::clamp(texel, 0, size - 1) : ((texel % size) + size) % size;
            return depth > row[static_cast<std::size_t>(wrapped)] + ShadowMap::getBias(light, axis);
        }
    };

    // A 40 by 40 box around a center, wound clockwise on screen.
    [[nodiscard]] static Occluder box(haylen::math::Vec2 center, Occluder::Cull cull = Occluder::Cull::Disabled) {
        return {.points = {{-20.0F, -20.0F}, {20.0F, -20.0F}, {20.0F, 20.0F}, {-20.0F, 20.0F}}, .cull = cull, .position = center};
    }
};

TEST(LightFlickerTest, WaversBelowOneDeterministically) {
    float lowest = 2.0F;
    float highest = 0.0F;
    for (int step = 0; step < 200; ++step) {
        const float value = LightFlicker::intensity(static_cast<float>(step) * 0.05F, 8.0F, 0.2F, 3);
        lowest = std::min(lowest, value);
        highest = std::max(highest, value);
    }

    EXPECT_GE(lowest, 0.8F);
    EXPECT_LE(highest, 1.0F);
    EXPECT_GT(highest - lowest, 0.05F);
    EXPECT_EQ(LightFlicker::intensity(1.5F, 8.0F, 0.2F, 3), LightFlicker::intensity(1.5F, 8.0F, 0.2F, 3));
    EXPECT_EQ(LightFlicker::intensity(1.5F, 8.0F, 0.0F, 3), 1.0F);

    // Many lights with their own seeds, more than a thread keeps noise for, flicker the same way whether their noise was kept or built again.
    std::vector<float> forward(20);
    std::vector<float> backward(20);

    for (std::size_t seed = 0; seed < forward.size(); ++seed) {
        forward[seed] = LightFlicker::intensity(0.7F, 8.0F, 0.5F, seed);
    }

    for (std::size_t seed = backward.size(); seed > 0; --seed) {
        backward[seed - 1] = LightFlicker::intensity(0.7F, 8.0F, 0.5F, seed - 1);
    }

    EXPECT_EQ(forward, backward);
    EXPECT_NE(forward[1], forward[2]);
}

TEST(LightTest, ResolvesNamesAndRejectsValuesOutOfRange) {
    EXPECT_EQ(Light::typeFromName("spot"), Light::Type::Spot);
    EXPECT_EQ(Light::typeName(Light::Type::Directional), "directional");
    EXPECT_EQ(Light::blendFromName("subtract"), Light::Blend::Subtract);
    EXPECT_EQ(Light::blendName(Light::Blend::Mix), "mix");
    EXPECT_EQ(Light::shadowFilterFromName("pcf13"), Light::ShadowFilter::Pcf13);
    EXPECT_EQ(Light::shadowFilterName(Light::ShadowFilter::Pcf5), "pcf5");
    EXPECT_FALSE(Light::typeFromName("area").has_value());
    EXPECT_EQ(Occluder::cullFromName("counterClockwise"), Occluder::Cull::CounterClockwise);
    EXPECT_EQ(Occluder::cullName(Occluder::Cull::Clockwise), "clockwise");

    EXPECT_NO_THROW(Light{}.validate());
    EXPECT_NO_THROW((Light{.type = Light::Type::Directional, .radius = 0.0F}).validate());
    EXPECT_THROW((Light{.radius = 0.0F}).validate(), std::invalid_argument);
    EXPECT_THROW((Light{.intensity = -1.0F}).validate(), std::invalid_argument);
    EXPECT_THROW((Light{.scale = {0.0F, 1.0F}}).validate(), std::invalid_argument);
    EXPECT_THROW((Light{.type = Light::Type::Spot, .innerAngle = 1.0F, .outerAngle = 0.5F}).validate(), std::invalid_argument);
    EXPECT_THROW((Light{.type = Light::Type::Spot, .outerAngle = 7.0F}).validate(), std::invalid_argument);
    EXPECT_THROW((Light{.height = -1.0F}).validate(), std::invalid_argument);
    EXPECT_THROW((Light{.layerMin = 2, .layerMax = 1}).validate(), std::invalid_argument);
    EXPECT_THROW((Light{.shadowSmoothness = -1.0F}).validate(), std::invalid_argument);
}

TEST(LightTest, FallsOffAroundPointsInsideSpotConesAndEverywhereFromDirections) {
    const Light point{.position = {100.0F, 100.0F}, .radius = 50.0F};
    EXPECT_FLOAT_EQ(point.getStrengthAt({100.0F, 100.0F}), 1.0F);
    EXPECT_FLOAT_EQ(point.getStrengthAt({125.0F, 100.0F}), Light::falloff(0.5F));
    EXPECT_FLOAT_EQ(point.getStrengthAt({160.0F, 100.0F}), 0.0F);
    EXPECT_GT(Light::falloff(0.25F), Light::falloff(0.75F));

    // A stretched light reaches farther along its stretched axis, turned with its rotation.
    const Light stretched{.position = {0.0F, 0.0F}, .radius = 50.0F, .rotation = haylen::math::Math::kHalfPi, .scale = {2.0F, 1.0F}};
    EXPECT_GT(stretched.getStrengthAt({0.0F, 75.0F}), 0.0F);
    EXPECT_FLOAT_EQ(stretched.getStrengthAt({75.0F, 0.0F}), 0.0F);

    // The spot points right with a cone of 60 degrees fully lit inside 30 degrees.
    const Light spot{.type = Light::Type::Spot, .radius = 100.0F, .innerAngle = haylen::math::Math::radians(30.0F), .outerAngle = haylen::math::Math::radians(60.0F)};
    EXPECT_FLOAT_EQ(spot.getStrengthAt(haylen::math::Vec2::fromAngle(haylen::math::Math::radians(10.0F), 50.0F)), Light::falloff(0.5F));
    EXPECT_FLOAT_EQ(spot.getStrengthAt(haylen::math::Vec2::fromAngle(haylen::math::Math::radians(40.0F), 50.0F)), 0.0F);
    const float edge = spot.getStrengthAt(haylen::math::Vec2::fromAngle(haylen::math::Math::radians(22.0F), 50.0F));
    EXPECT_GT(edge, 0.0F);
    EXPECT_LT(edge, Light::falloff(0.5F));
    EXPECT_FLOAT_EQ(spot.getStrengthAt({-50.0F, 0.0F}), 0.0F);

    const Light directional{.type = Light::Type::Directional, .rotation = 1.0F};
    EXPECT_FLOAT_EQ(directional.getStrengthAt({-5000.0F, 9000.0F}), 1.0F);
}

TEST(LightTest, CombinesBlendModesInDrawOrderBeyondOne) {
    const haylen::math::Color ambient{0.2F, 0.2F, 0.2F, 1.0F};
    const Light bright{.radius = 100.0F, .intensity = 3.0F};
    const Light dimmer{.radius = 100.0F, .color = haylen::math::Color::white(), .intensity = 0.5F, .blend = Light::Blend::Subtract};
    const Light red{.radius = 100.0F, .color = {1.0F, 0.0F, 0.0F, 1.0F}, .blend = Light::Blend::Mix};

    // Lights add over the ambient color without saturating, subtract below it and mix toward their own color by their strength.
    EXPECT_FLOAT_EQ(bright.apply(ambient, {}).r, 3.2F);
    EXPECT_FLOAT_EQ(dimmer.apply(ambient, {}).g, -0.3F);
    EXPECT_EQ(red.apply(ambient, {}), (haylen::math::Color{1.0F, 0.0F, 0.0F, 1.0F}));
    EXPECT_EQ(red.apply(ambient, {200.0F, 0.0F}), ambient);
    EXPECT_EQ((Light{.enabled = false}).apply(ambient, {}), ambient);

    const std::vector<Light> lights{bright, red, dimmer};
    const haylen::math::Color lit = Light::illuminate(ambient, lights, {});
    EXPECT_FLOAT_EQ(lit.r, 0.5F);
    EXPECT_FLOAT_EQ(lit.g, -0.5F);
    EXPECT_EQ(Light::illuminate(ambient, {}, {}), ambient);
}

TEST(LightTest, KeepsTheBrighterChannelWithTheMaximumBlend) {
    EXPECT_EQ(Light::blendFromName("max"), Light::Blend::Max);
    const Light warm{.radius = 100.0F, .color = {1.0F, 0.5F, 0.0F, 1.0F}, .intensity = 0.8F, .blend = Light::Blend::Max};
    const Light torch{.position = {40.0F, 0.0F}, .radius = 100.0F, .color = {0.6F, 0.6F, 0.6F, 1.0F}, .blend = Light::Blend::Max};
    const haylen::math::Color dark{0.1F, 0.1F, 0.3F, 1.0F};
    const haylen::math::Color lit = Light::illuminate(dark, std::vector<Light>{warm, torch}, {});
    EXPECT_FLOAT_EQ(lit.r, 0.8F);
    EXPECT_FLOAT_EQ(lit.g, std::max(0.4F, 0.6F * torch.getStrengthAt({})));
    EXPECT_FLOAT_EQ(lit.b, 0.3F);
    const haylen::math::Color again = Light::illuminate(lit, std::vector<Light>{warm}, {});
    EXPECT_EQ(again, lit);
}

TEST(LightTest, ReachesDrawsByLightMaskAndLayerRange) {
    const Light light{.radius = 100.0F, .itemMask = 0b0110, .layerMin = -1, .layerMax = 2};
    EXPECT_TRUE(light.affects(0b0010, 0));
    EXPECT_TRUE(light.affects(0b0100, 2));
    EXPECT_FALSE(light.affects(0b0001, 0));
    EXPECT_FALSE(light.affects(0b0010, 3));
    EXPECT_FALSE(light.affects(0b0010, -2));

    // Layers beyond the range lit canvases tell apart count as the nearest end, so a range from 200 reaches layer 900.
    const Light high{.layerMin = 200};
    EXPECT_TRUE(high.affects(1, 900));
    EXPECT_TRUE(high.affects(1, Light::kHighestLayer));
    EXPECT_FALSE(high.affects(1, 126));

    const haylen::math::Color ambient = haylen::math::Color::black();
    const std::vector<Light> lights{light};
    EXPECT_FLOAT_EQ(Light::illuminate(ambient, lights, {}, 0b0010, 0).r, 1.0F);
    EXPECT_FLOAT_EQ(Light::illuminate(ambient, lights, {}, 0b0001, 0).r, 0.0F);
}

TEST_F(ShadowMapTest, ShadowsWhatLiesBehindOccludersOfPointLights) {
    const Light light{.position = {100.0F, 100.0F}, .radius = 400.0F, .shadows = true};
    const std::vector<Occluder> occluders{box({300.0F, 100.0F})};
    const CastRow cast(light, occluders);

    // A pixel behind the box is shadowed, and pixels in front of it or beside its shadow are lit, in the map and in the exact query.
    for (const haylen::math::Vec2 point : {haylen::math::Vec2{400.0F, 100.0F}, haylen::math::Vec2{450.0F, 120.0F}}) {
        EXPECT_TRUE(cast.shadows(light, point));
        EXPECT_TRUE(light.isShadowedAt(point, occluders));
    }

    for (const haylen::math::Vec2 point : {haylen::math::Vec2{200.0F, 100.0F}, haylen::math::Vec2{400.0F, 300.0F}, haylen::math::Vec2{100.0F, 300.0F}}) {
        EXPECT_FALSE(cast.shadows(light, point));
        EXPECT_FALSE(light.isShadowedAt(point, occluders));
    }

    // Occluders far beyond the reach of the light cast nothing, and lights without shadows never shadow.
    const CastRow far(light, {box({900.0F, 900.0F})});
    EXPECT_TRUE(std::all_of(far.row.begin(), far.row.end(), [](float depth) { return depth == ShadowMap::kClear; }));
    EXPECT_FALSE((Light{.position = {100.0F, 100.0F}, .radius = 400.0F}).isShadowedAt({400.0F, 100.0F}, occluders));

    // An occluder with too few points is rejected before it could cast anything.
    const std::vector<Occluder> empty{{.closed = false}};
    EXPECT_THROW((void)light.isShadowedAt({400.0F, 100.0F}, empty), std::invalid_argument);
}

// A light on a corner or on an edge of an occluder sees the edges through it from the side, so they shadow no direction, while the other edges still cast.
// After the map, every smaller copy keeps in each texel the nearest occluder of the two texels it covers, so the soft filter reads wide windows from few texels and never misses an occluder, even one a texel thin.
TEST_F(ShadowMapTest, KeepsTheNearestOccluderInEverySmallerCopy) {
    const Light light{.position = {500.0F, 500.0F}, .radius = 400.0F, .shadows = true};
    const CastRow cast(light, {box({700.0F, 500.0F}), {.points = {{-1.0F, 0.0F}, {1.0F, 0.0F}}, .closed = false, .position = {500.0F, 200.0F}}});
    const std::vector<float>& row = cast.row;
    std::size_t from = 0;
    std::size_t width = ShadowMap::kResolution;
    for (int level = 1; level < ShadowMap::kLevels; ++level) {
        for (std::size_t texel = 0; texel < width / 2; ++texel) {
            ASSERT_EQ(row[from + width + texel], std::min(row[from + texel * 2], row[from + texel * 2 + 1])) << level;
        }

        from += width;
        width /= 2;
    }

    EXPECT_LE(from + width, row.size());

    // The occluder a texel thin above the light still shows in the smallest copy, whose texel above the light is as near as the occluder.
    const std::size_t above = from + width / 4;
    EXPECT_NEAR(row[above], 300.0F / 400.0F, 0.01F);
}

TEST_F(ShadowMapTest, LightsAroundTheEdgesThatPassThroughTheLight) {
    const Light light{.position = {100.0F, 100.0F}, .radius = 400.0F, .shadows = true};
    const std::vector<Occluder> corner{box({80.0F, 120.0F})};
    const CastRow cornerCast(light, corner);
    for (const haylen::math::Vec2 point : {haylen::math::Vec2{130.0F, 130.0F}, haylen::math::Vec2{130.0F, 70.0F}, haylen::math::Vec2{70.0F, 70.0F}}) {
        EXPECT_FALSE(cornerCast.shadows(light, point));
        EXPECT_FALSE(light.isShadowedAt(point, corner));
    }

    EXPECT_TRUE(light.isShadowedAt({60.0F, 160.0F}, corner));

    const std::vector<Occluder> edge{box({100.0F, 120.0F})};
    const CastRow edgeCast(light, edge);
    for (const haylen::math::Vec2 point : {haylen::math::Vec2{100.0F, 50.0F}, haylen::math::Vec2{160.0F, 80.0F}}) {
        EXPECT_FALSE(edgeCast.shadows(light, point));
        EXPECT_FALSE(light.isShadowedAt(point, edge));
    }

    EXPECT_TRUE(edgeCast.shadows(light, {100.0F, 200.0F}));
    EXPECT_TRUE(light.isShadowedAt({100.0F, 200.0F}, edge));
}

TEST_F(ShadowMapTest, SkipsEdgesByCullModeAndMask) {
    const Light light{.position = {100.0F, 100.0F}, .radius = 400.0F, .shadows = true};
    const haylen::math::Vec2 inside{300.0F, 100.0F};
    const haylen::math::Vec2 behind{400.0F, 100.0F};

    // Without culling the box shadows itself, counterclockwise culling skips the edge facing the light so only what lies behind is dark, and clockwise culling keeps only that edge.
    EXPECT_TRUE(CastRow(light, {box(inside)}).shadows(light, inside));
    const std::vector<Occluder> backOnly{box(inside, Occluder::Cull::CounterClockwise)};
    EXPECT_FALSE(CastRow(light, backOnly).shadows(light, inside));
    EXPECT_TRUE(CastRow(light, backOnly).shadows(light, behind));
    EXPECT_FALSE(light.isShadowedAt(inside, backOnly));
    const std::vector<Occluder> frontOnly{box(inside, Occluder::Cull::Clockwise)};
    EXPECT_TRUE(CastRow(light, frontOnly).shadows(light, inside));
    EXPECT_TRUE(light.isShadowedAt(inside, frontOnly));

    // Occluders whose mask shares no bit with the shadow mask of the light cast nothing, and open occluders skip the closing edge.
    Occluder masked = box(inside);
    masked.mask = 0b10;
    EXPECT_FALSE(CastRow(light, {masked}).shadows(light, behind));
    EXPECT_FALSE(light.isShadowedAt(behind, {&masked, 1}));
    const Occluder open{.points = {{280.0F, 50.0F}, {280.0F, 80.0F}}, .closed = false};
    EXPECT_TRUE(CastRow(light, {open}).shadows(light, {400.0F, 30.0F}));
    EXPECT_FALSE(CastRow(light, {open}).shadows(light, behind));
}

TEST_F(ShadowMapTest, CastsParallelShadowsOfDirectionalLights) {
    // The sun travels down and to the right at 45 degrees.
    const Light sun{.type = Light::Type::Directional, .rotation = haylen::math::Math::kPi * 0.25F, .shadows = true};
    const std::vector<Occluder> occluders{box({200.0F, 200.0F})};
    const CastRow cast(sun, occluders);

    EXPECT_TRUE(cast.shadows(sun, {400.0F, 400.0F}));
    EXPECT_TRUE(cast.shadows(sun, {900.0F, 900.0F}));
    EXPECT_FALSE(cast.shadows(sun, {100.0F, 100.0F}));
    EXPECT_FALSE(cast.shadows(sun, {400.0F, 200.0F}));
    EXPECT_TRUE(sun.isShadowedAt({410.0F, 400.0F}, occluders));
    EXPECT_FALSE(sun.isShadowedAt({400.0F, 200.0F}, occluders));
    EXPECT_GT(cast.axis.acrossSpan, 1000.0F);
    EXPECT_GT(ShadowMap::getBias(sun, cast.axis), 0.0F);
}

TEST(OccluderTest, PlacesPointsAndBuildsFromBodiesAndMaps) {
    const Occluder turned{.points = {{10.0F, 0.0F}, {10.0F, 5.0F}}, .closed = false, .position = {100.0F, 100.0F}, .rotation = haylen::math::Math::kHalfPi, .scale = {2.0F, 1.0F}};
    const std::vector<haylen::math::Vec2> placed = turned.getWorldPoints();
    EXPECT_NEAR(placed[0].x, 100.0F, 1e-4F);
    EXPECT_NEAR(placed[0].y, 120.0F, 1e-4F);
    EXPECT_NEAR(placed[1].x, 95.0F, 1e-4F);
    EXPECT_NO_THROW(turned.validate());
    EXPECT_THROW((Occluder{.points = {{0.0F, 0.0F}, {1.0F, 0.0F}}}).validate(), std::invalid_argument);
    EXPECT_THROW((Occluder{.points = {{0.0F, 0.0F}}, .closed = false}).validate(), std::invalid_argument);

    // A body gives one occluder per shape in its own space, and the segments of a chain join into one outline, closed for loops. Open chains leave their end points out of the collision, as Box2D uses them as ghost vertices.
    haylen::physics2d::World world({.gravity = {}});
    haylen::physics2d::Body body = world.createBody({.position = {50.0F, 60.0F}, .rotation = 0.5F});
    body.addBox({20.0F, 10.0F});
    body.addCircle(8.0F, {.offset = {30.0F, 0.0F}});
    body.addSegment({0.0F, 0.0F}, {0.0F, 40.0F});
    body.addCapsule({-10.0F, 0.0F}, {-30.0F, 0.0F}, 4.0F);
    const std::vector<haylen::math::Vec2> chain{{0.0F, 100.0F}, {50.0F, 100.0F}, {100.0F, 120.0F}, {150.0F, 100.0F}, {200.0F, 90.0F}, {250.0F, 100.0F}};
    haylen::physics2d::Body ground = world.createBody({.type = haylen::physics2d::Body::Type::Static});
    ground.addChain(chain, false);
    haylen::physics2d::Body pit = world.createBody({.type = haylen::physics2d::Body::Type::Static});
    pit.addChain(std::vector<haylen::math::Vec2>{{0.0F, 0.0F}, {0.0F, 50.0F}, {50.0F, 50.0F}, {50.0F, 0.0F}}, true);

    const std::vector<Occluder> shapes = Occluder::fromBody(world, body);
    ASSERT_EQ(shapes.size(), 4U);
    EXPECT_EQ(std::count_if(shapes.begin(), shapes.end(), [](const Occluder& occluder) { return occluder.closed; }), 3);

    for (const Occluder& occluder : shapes) {
        EXPECT_NEAR(occluder.position.x, 50.0F, 1e-3F);
        EXPECT_NEAR(occluder.rotation, 0.5F, 1e-3F);
        EXPECT_NO_THROW(occluder.validate());
    }

    const std::vector<Occluder> outline = Occluder::fromBody(world, ground);
    ASSERT_EQ(outline.size(), 1U);
    EXPECT_FALSE(outline[0].closed);
    EXPECT_EQ(outline[0].points.size(), chain.size());
    const std::vector<Occluder> loop = Occluder::fromBody(world, pit);
    ASSERT_EQ(loop.size(), 1U);
    EXPECT_TRUE(loop[0].closed);
    EXPECT_EQ(loop[0].points.size(), 4U);

    // A map gives one occluder per object with an outline, with polylines open and points left out.
    haylen::tiled::Map map;
    map.width = 4;
    map.height = 4;
    map.tileSize = {16.0F, 16.0F};
    haylen::tiled::Layer walls{.name = "walls", .kind = haylen::tiled::Layer::Kind::Object, .offset = {4.0F, 2.0F}};
    walls.objects = {{.id = 1, .position = {10.0F, 10.0F}, .size = {20.0F, 10.0F}}, {.id = 2, .position = {40.0F, 0.0F}, .shape = haylen::tiled::Object::Shape::Polyline, .points = {{0.0F, 0.0F}, {0.0F, 30.0F}}}, {.id = 3, .position = {5.0F, 5.0F}, .shape = haylen::tiled::Object::Shape::Point}};
    map.layers = {walls};
    const haylen::tiled::MapRenderer renderer(std::move(map));
    const std::vector<Occluder> objects = Occluder::fromMap(renderer, "walls");
    ASSERT_EQ(objects.size(), 2U);
    EXPECT_TRUE(objects[0].closed);
    EXPECT_EQ(objects[0].points.front(), (haylen::math::Vec2{14.0F, 12.0F}));
    EXPECT_FALSE(objects[1].closed);
    EXPECT_EQ(objects[1].points.back(), (haylen::math::Vec2{44.0F, 32.0F}));
    EXPECT_THROW((void)Occluder::fromMap(renderer, "missing"), std::invalid_argument);
}

} // namespace haylen::lighting2d
