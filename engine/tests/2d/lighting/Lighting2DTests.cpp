#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

#include "2d/lighting/ShadowMap.hpp"
#include "haylen/2d/lighting/Light.hpp"
#include "haylen/2d/lighting/LightFlicker.hpp"
#include "haylen/2d/lighting/Occluder.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/2d/tiled/MapRenderer.hpp"
#include "haylen/math/Math.hpp"
#include "support/EngineFixture.hpp"

namespace haylen {

namespace {

// Casts the shadow map row of a light over some occluders, the way the renderer does for a canvas that shows the bounds, and reads it the way the light shader does.
struct CastRow {
    std::vector<float> row = std::vector<float>(lighting2d::ShadowMap::kResolution);
    lighting2d::ShadowMap::Axis axis{};

    CastRow(const lighting2d::Light& light, const std::vector<lighting2d::Occluder>& occluders, math::Rect bounds = {0.0F, 0.0F, 1000.0F, 1000.0F}) {
        std::vector<lighting2d::ShadowMap::Segment> segments;
        for (const lighting2d::Occluder& occluder : occluders) {
            lighting2d::ShadowMap::appendSegments(occluder, segments);
        }
        axis = lighting2d::ShadowMap::cast(light, segments, bounds, row);
    }

    // The shader finds the texel of the direction or of the place across the light, and a point deeper than the occluder depth there plus the bias is shadowed.
    [[nodiscard]] bool shadows(const lighting2d::Light& light, math::Vec2 point) const {
        const auto width = static_cast<float>(row.size());
        float coordinate = 0.0F;
        float depth = 0.0F;
        if (light.type == lighting2d::Light::Type::Directional) {
            const math::Vec2 direction = math::Vec2::fromAngle(light.rotation);
            coordinate = (math::Vec2::dot(point, direction.getPerpendicular()) - axis.acrossStart) / axis.acrossSpan * width;
            depth = (math::Vec2::dot(point, direction) - axis.alongStart) / axis.alongSpan;
        } else {
            const math::Vec2 offset = point - light.position;
            coordinate = (std::atan2(offset.y, offset.x) / math::Math::kTau + 0.5F) * width;
            depth = offset.getLength() / lighting2d::ShadowMap::getRange(light);
        }

        const auto size = static_cast<int>(row.size());
        const int texel = static_cast<int>(std::floor(coordinate));
        const int wrapped = light.type == lighting2d::Light::Type::Directional ? std::clamp(texel, 0, size - 1) : ((texel % size) + size) % size;
        return depth > row[static_cast<std::size_t>(wrapped)] + lighting2d::ShadowMap::getBias(light, axis);
    }
};

class ShadowMapTest : public ::testing::Test {
  protected:
    // A 40 by 40 box around a center, wound clockwise on screen.
    [[nodiscard]] static lighting2d::Occluder box(math::Vec2 center, lighting2d::Occluder::Cull cull = lighting2d::Occluder::Cull::Disabled) {
        return {.points = {{-20.0F, -20.0F}, {20.0F, -20.0F}, {20.0F, 20.0F}, {-20.0F, 20.0F}}, .cull = cull, .position = center};
    }
};

} // namespace

TEST(LightFlickerTest, WaversBelowOneDeterministically) {
    float lowest = 2.0F;
    float highest = 0.0F;
    for (int step = 0; step < 200; ++step) {
        const float value = lighting2d::LightFlicker::intensity(static_cast<float>(step) * 0.05F, 8.0F, 0.2F, 3);
        lowest = std::min(lowest, value);
        highest = std::max(highest, value);
    }
    EXPECT_GE(lowest, 0.8F);
    EXPECT_LE(highest, 1.0F);
    EXPECT_GT(highest - lowest, 0.05F);
    EXPECT_EQ(lighting2d::LightFlicker::intensity(1.5F, 8.0F, 0.2F, 3), lighting2d::LightFlicker::intensity(1.5F, 8.0F, 0.2F, 3));
    EXPECT_EQ(lighting2d::LightFlicker::intensity(1.5F, 8.0F, 0.0F, 3), 1.0F);

    // Many lights with their own seeds, more than a thread keeps noise for, flicker the same way whether their noise was kept or built again.
    std::vector<float> forward(20);
    std::vector<float> backward(20);
    for (std::size_t seed = 0; seed < forward.size(); ++seed) {
        forward[seed] = lighting2d::LightFlicker::intensity(0.7F, 8.0F, 0.5F, seed);
    }
    for (std::size_t seed = backward.size(); seed > 0; --seed) {
        backward[seed - 1] = lighting2d::LightFlicker::intensity(0.7F, 8.0F, 0.5F, seed - 1);
    }
    EXPECT_EQ(forward, backward);
    EXPECT_NE(forward[1], forward[2]);
}

TEST(LightTest, ResolvesNamesAndRejectsValuesOutOfRange) {
    using Light = lighting2d::Light;
    EXPECT_EQ(Light::typeFromName("spot"), Light::Type::Spot);
    EXPECT_EQ(Light::typeName(Light::Type::Directional), "directional");
    EXPECT_EQ(Light::blendFromName("subtract"), Light::Blend::Subtract);
    EXPECT_EQ(Light::blendName(Light::Blend::Mix), "mix");
    EXPECT_EQ(Light::shadowFilterFromName("pcf13"), Light::ShadowFilter::Pcf13);
    EXPECT_EQ(Light::shadowFilterName(Light::ShadowFilter::Pcf5), "pcf5");
    EXPECT_FALSE(Light::typeFromName("area").has_value());
    EXPECT_EQ(lighting2d::Occluder::cullFromName("counterClockwise"), lighting2d::Occluder::Cull::CounterClockwise);
    EXPECT_EQ(lighting2d::Occluder::cullName(lighting2d::Occluder::Cull::Clockwise), "clockwise");

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
    using Light = lighting2d::Light;
    const Light point{.position = {100.0F, 100.0F}, .radius = 50.0F};
    EXPECT_FLOAT_EQ(point.getStrengthAt({100.0F, 100.0F}), 1.0F);
    EXPECT_FLOAT_EQ(point.getStrengthAt({125.0F, 100.0F}), Light::falloff(0.5F));
    EXPECT_FLOAT_EQ(point.getStrengthAt({160.0F, 100.0F}), 0.0F);
    EXPECT_GT(Light::falloff(0.25F), Light::falloff(0.75F));

    // A stretched light reaches farther along its stretched axis, turned with its rotation.
    const Light stretched{.position = {0.0F, 0.0F}, .radius = 50.0F, .rotation = math::Math::kHalfPi, .scale = {2.0F, 1.0F}};
    EXPECT_GT(stretched.getStrengthAt({0.0F, 75.0F}), 0.0F);
    EXPECT_FLOAT_EQ(stretched.getStrengthAt({75.0F, 0.0F}), 0.0F);

    // The spot points right with a cone of 60 degrees fully lit inside 30 degrees.
    const Light spot{.type = Light::Type::Spot, .radius = 100.0F, .innerAngle = math::Math::radians(30.0F), .outerAngle = math::Math::radians(60.0F)};
    EXPECT_FLOAT_EQ(spot.getStrengthAt(math::Vec2::fromAngle(math::Math::radians(10.0F), 50.0F)), Light::falloff(0.5F));
    EXPECT_FLOAT_EQ(spot.getStrengthAt(math::Vec2::fromAngle(math::Math::radians(40.0F), 50.0F)), 0.0F);
    const float edge = spot.getStrengthAt(math::Vec2::fromAngle(math::Math::radians(22.0F), 50.0F));
    EXPECT_GT(edge, 0.0F);
    EXPECT_LT(edge, Light::falloff(0.5F));
    EXPECT_FLOAT_EQ(spot.getStrengthAt({-50.0F, 0.0F}), 0.0F);

    const Light directional{.type = Light::Type::Directional, .rotation = 1.0F};
    EXPECT_FLOAT_EQ(directional.getStrengthAt({-5000.0F, 9000.0F}), 1.0F);
}

TEST(LightTest, CombinesBlendModesInDrawOrderBeyondOne) {
    using Light = lighting2d::Light;
    const math::Color ambient{0.2F, 0.2F, 0.2F, 1.0F};
    const Light bright{.radius = 100.0F, .intensity = 3.0F};
    const Light dimmer{.radius = 100.0F, .color = math::Color::white(), .intensity = 0.5F, .blend = Light::Blend::Subtract};
    const Light red{.radius = 100.0F, .color = {1.0F, 0.0F, 0.0F, 1.0F}, .blend = Light::Blend::Mix};

    // Lights add over the ambient color without saturating, subtract below it and mix toward their own color by their strength.
    EXPECT_FLOAT_EQ(bright.apply(ambient, {}).r, 3.2F);
    EXPECT_FLOAT_EQ(dimmer.apply(ambient, {}).g, -0.3F);
    EXPECT_EQ(red.apply(ambient, {}), (math::Color{1.0F, 0.0F, 0.0F, 1.0F}));
    EXPECT_EQ(red.apply(ambient, {200.0F, 0.0F}), ambient);
    EXPECT_EQ((Light{.enabled = false}).apply(ambient, {}), ambient);

    const std::vector<Light> lights{bright, red, dimmer};
    const math::Color lit = Light::illuminate(ambient, lights, {});
    EXPECT_FLOAT_EQ(lit.r, 0.5F);
    EXPECT_FLOAT_EQ(lit.g, -0.5F);
    EXPECT_EQ(Light::illuminate(ambient, {}, {}), ambient);
}

TEST(LightTest, ReachesDrawsByLightMaskAndLayerRange) {
    using Light = lighting2d::Light;
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

    const math::Color ambient = math::Color::black();
    const std::vector<Light> lights{light};
    EXPECT_FLOAT_EQ(Light::illuminate(ambient, lights, {}, 0b0010, 0).r, 1.0F);
    EXPECT_FLOAT_EQ(Light::illuminate(ambient, lights, {}, 0b0001, 0).r, 0.0F);
}

TEST_F(ShadowMapTest, ShadowsWhatLiesBehindOccludersOfPointLights) {
    const lighting2d::Light light{.position = {100.0F, 100.0F}, .radius = 400.0F, .shadows = true};
    const std::vector<lighting2d::Occluder> occluders{box({300.0F, 100.0F})};
    const CastRow cast(light, occluders);

    // A pixel behind the box is shadowed, and pixels in front of it or beside its shadow are lit, in the map and in the exact query.
    for (const math::Vec2 point : {math::Vec2{400.0F, 100.0F}, math::Vec2{450.0F, 120.0F}}) {
        EXPECT_TRUE(cast.shadows(light, point));
        EXPECT_TRUE(light.isShadowedAt(point, occluders));
    }
    for (const math::Vec2 point : {math::Vec2{200.0F, 100.0F}, math::Vec2{400.0F, 300.0F}, math::Vec2{100.0F, 300.0F}}) {
        EXPECT_FALSE(cast.shadows(light, point));
        EXPECT_FALSE(light.isShadowedAt(point, occluders));
    }

    // Occluders far beyond the reach of the light cast nothing, and lights without shadows never shadow.
    const CastRow far(light, {box({900.0F, 900.0F})});
    EXPECT_TRUE(std::all_of(far.row.begin(), far.row.end(), [](float depth) { return depth == lighting2d::ShadowMap::kClear; }));
    EXPECT_FALSE((lighting2d::Light{.position = {100.0F, 100.0F}, .radius = 400.0F}).isShadowedAt({400.0F, 100.0F}, occluders));

    // An occluder with too few points is rejected before it could cast anything.
    const std::vector<lighting2d::Occluder> empty{{.closed = false}};
    EXPECT_THROW((void)light.isShadowedAt({400.0F, 100.0F}, empty), std::invalid_argument);
}

TEST_F(ShadowMapTest, SkipsEdgesByCullModeAndMask) {
    const lighting2d::Light light{.position = {100.0F, 100.0F}, .radius = 400.0F, .shadows = true};
    const math::Vec2 inside{300.0F, 100.0F};
    const math::Vec2 behind{400.0F, 100.0F};

    // Without culling the box shadows itself, counterclockwise culling skips the edge facing the light so only what lies behind is dark, and clockwise culling keeps only that edge.
    EXPECT_TRUE(CastRow(light, {box(inside)}).shadows(light, inside));
    const std::vector<lighting2d::Occluder> backOnly{box(inside, lighting2d::Occluder::Cull::CounterClockwise)};
    EXPECT_FALSE(CastRow(light, backOnly).shadows(light, inside));
    EXPECT_TRUE(CastRow(light, backOnly).shadows(light, behind));
    EXPECT_FALSE(light.isShadowedAt(inside, backOnly));
    const std::vector<lighting2d::Occluder> frontOnly{box(inside, lighting2d::Occluder::Cull::Clockwise)};
    EXPECT_TRUE(CastRow(light, frontOnly).shadows(light, inside));
    EXPECT_TRUE(light.isShadowedAt(inside, frontOnly));

    // Occluders whose mask shares no bit with the shadow mask of the light cast nothing, and open occluders skip the closing edge.
    lighting2d::Occluder masked = box(inside);
    masked.mask = 0b10;
    EXPECT_FALSE(CastRow(light, {masked}).shadows(light, behind));
    EXPECT_FALSE(light.isShadowedAt(behind, {&masked, 1}));
    const lighting2d::Occluder open{.points = {{280.0F, 50.0F}, {280.0F, 80.0F}}, .closed = false};
    EXPECT_TRUE(CastRow(light, {open}).shadows(light, {400.0F, 30.0F}));
    EXPECT_FALSE(CastRow(light, {open}).shadows(light, behind));
}

TEST_F(ShadowMapTest, CastsParallelShadowsOfDirectionalLights) {
    // The sun travels down and to the right at 45 degrees.
    const lighting2d::Light sun{.type = lighting2d::Light::Type::Directional, .rotation = math::Math::kPi * 0.25F, .shadows = true};
    const std::vector<lighting2d::Occluder> occluders{box({200.0F, 200.0F})};
    const CastRow cast(sun, occluders);

    EXPECT_TRUE(cast.shadows(sun, {400.0F, 400.0F}));
    EXPECT_TRUE(cast.shadows(sun, {900.0F, 900.0F}));
    EXPECT_FALSE(cast.shadows(sun, {100.0F, 100.0F}));
    EXPECT_FALSE(cast.shadows(sun, {400.0F, 200.0F}));
    EXPECT_TRUE(sun.isShadowedAt({410.0F, 400.0F}, occluders));
    EXPECT_FALSE(sun.isShadowedAt({400.0F, 200.0F}, occluders));
    EXPECT_GT(cast.axis.acrossSpan, 1000.0F);
    EXPECT_GT(lighting2d::ShadowMap::getBias(sun, cast.axis), 0.0F);
}

TEST(OccluderTest, PlacesPointsAndBuildsFromBodiesAndMaps) {
    const lighting2d::Occluder turned{.points = {{10.0F, 0.0F}, {10.0F, 5.0F}}, .closed = false, .position = {100.0F, 100.0F}, .rotation = math::Math::kHalfPi, .scale = {2.0F, 1.0F}};
    const std::vector<math::Vec2> placed = turned.getWorldPoints();
    EXPECT_NEAR(placed[0].x, 100.0F, 1e-4F);
    EXPECT_NEAR(placed[0].y, 120.0F, 1e-4F);
    EXPECT_NEAR(placed[1].x, 95.0F, 1e-4F);
    EXPECT_NO_THROW(turned.validate());
    EXPECT_THROW((lighting2d::Occluder{.points = {{0.0F, 0.0F}, {1.0F, 0.0F}}}).validate(), std::invalid_argument);
    EXPECT_THROW((lighting2d::Occluder{.points = {{0.0F, 0.0F}}, .closed = false}).validate(), std::invalid_argument);

    // A body gives one occluder per shape in its own space, and the segments of a chain join into one outline, closed for loops. Open chains leave their end points out of the collision, as Box2D uses them as ghost vertices.
    physics2d::World world({.gravity = {}});
    physics2d::Body body = world.createBody({.position = {50.0F, 60.0F}, .rotation = 0.5F});
    body.addBox({20.0F, 10.0F});
    body.addCircle(8.0F, {.offset = {30.0F, 0.0F}});
    body.addSegment({0.0F, 0.0F}, {0.0F, 40.0F});
    body.addCapsule({-10.0F, 0.0F}, {-30.0F, 0.0F}, 4.0F);
    const std::vector<math::Vec2> chain{{0.0F, 100.0F}, {50.0F, 100.0F}, {100.0F, 120.0F}, {150.0F, 100.0F}, {200.0F, 90.0F}, {250.0F, 100.0F}};
    physics2d::Body ground = world.createBody({.type = physics2d::Body::Type::Static});
    ground.addChain(chain, false);
    physics2d::Body pit = world.createBody({.type = physics2d::Body::Type::Static});
    pit.addChain(std::vector<math::Vec2>{{0.0F, 0.0F}, {0.0F, 50.0F}, {50.0F, 50.0F}, {50.0F, 0.0F}}, true);

    const std::vector<lighting2d::Occluder> shapes = lighting2d::Occluder::fromBody(world, body);
    ASSERT_EQ(shapes.size(), 4U);
    EXPECT_EQ(std::count_if(shapes.begin(), shapes.end(), [](const lighting2d::Occluder& occluder) { return occluder.closed; }), 3);
    for (const lighting2d::Occluder& occluder : shapes) {
        EXPECT_NEAR(occluder.position.x, 50.0F, 1e-3F);
        EXPECT_NEAR(occluder.rotation, 0.5F, 1e-3F);
        EXPECT_NO_THROW(occluder.validate());
    }
    const std::vector<lighting2d::Occluder> outline = lighting2d::Occluder::fromBody(world, ground);
    ASSERT_EQ(outline.size(), 1U);
    EXPECT_FALSE(outline[0].closed);
    EXPECT_EQ(outline[0].points.size(), chain.size() - 2);
    const std::vector<lighting2d::Occluder> loop = lighting2d::Occluder::fromBody(world, pit);
    ASSERT_EQ(loop.size(), 1U);
    EXPECT_TRUE(loop[0].closed);
    EXPECT_EQ(loop[0].points.size(), 4U);

    // A map gives one occluder per object with an outline, with polylines open and points left out.
    tiled::Map map;
    map.width = 4;
    map.height = 4;
    map.tileSize = {16.0F, 16.0F};
    tiled::Layer walls{.name = "walls", .kind = tiled::Layer::Kind::Object, .offset = {4.0F, 2.0F}};
    walls.objects = {{.id = 1, .position = {10.0F, 10.0F}, .size = {20.0F, 10.0F}}, {.id = 2, .position = {40.0F, 0.0F}, .shape = tiled::Object::Shape::Polyline, .points = {{0.0F, 0.0F}, {0.0F, 30.0F}}}, {.id = 3, .position = {5.0F, 5.0F}, .shape = tiled::Object::Shape::Point}};
    map.layers = {walls};
    const tiled::MapRenderer renderer(std::move(map));
    const std::vector<lighting2d::Occluder> objects = lighting2d::Occluder::fromMap(renderer, "walls");
    ASSERT_EQ(objects.size(), 2U);
    EXPECT_TRUE(objects[0].closed);
    EXPECT_EQ(objects[0].points.front(), (math::Vec2{14.0F, 12.0F}));
    EXPECT_FALSE(objects[1].closed);
    EXPECT_EQ(objects[1].points.back(), (math::Vec2{44.0F, 32.0F}));
    EXPECT_THROW((void)lighting2d::Occluder::fromMap(renderer, "missing"), std::invalid_argument);
}

TEST(Lighting2DLuaTest, FlickersFromLua) {
    test::EngineFixture fixture;
    fixture.runLua("lighting2d = require('haylen.lighting2d')");

    EXPECT_EQ(fixture.lua("local f = lighting2d.flicker(1.25, {speed = 6, amount = 0.1, seed = 4}) return f > 0.85 and f < 1.15 and lighting2d.flicker(0) > 0"), "true");
    EXPECT_EQ(fixture.lua("return lighting2d.flicker(2, {amount = 0}) == 1"), "true");
    EXPECT_NE(fixture.lua("lighting2d.flicker(1, {wobble = 1})").find("Unknown option \"wobble\""), std::string::npos);
    EXPECT_NE(fixture.lua("lighting2d.flicker(1, 'fast')").find("error: "), std::string::npos);
}

TEST(Lighting2DLuaTest, CreatesLightsAndAsksWhereTheyReach) {
    test::EngineFixture fixture;
    fixture.runLua(R"(
        lighting2d = require('haylen.lighting2d')
        graphics = require('haylen.graphics')
        torch = lighting2d.newLight({type = 'spot', x = 10, y = 20, radius = 200, color = '#FFFF8000', intensity = 2, innerAngle = 0.4, outerAngle = 0.9, height = 30, blend = 'mix', itemMask = 3, layerMin = -2, layerMax = 4, shadows = true, shadowFilter = 'pcf5', shadowColor = '#80000040', shadowSmoothness = 1.5, shadowMask = 6})
    )");

    EXPECT_EQ(fixture.lua("return torch.type .. ' ' .. torch.x .. ' ' .. torch.position.y .. ' ' .. torch.radius .. ' ' .. torch.intensity .. ' ' .. torch.blend .. ' ' .. torch.itemMask .. ' ' .. torch.layerMin .. ' ' .. torch.shadowFilter .. ' ' .. torch.shadowMask .. ' ' .. tostring(torch.shadows) .. ' ' .. tostring(torch.enabled) .. ' ' .. tostring(torch.texture)"), "spot 10.0 20.0 200.0 2.0 mix 3 -2 pcf5 6 true true nil");
    EXPECT_EQ(fixture.lua("torch.texture = graphics.whiteTexture() torch.scaleX = 2 torch.rotation = 1 return torch.texture.width .. ' ' .. torch.scaleX .. ' ' .. torch.rotation"), "1 2.0 1.0");
    EXPECT_EQ(fixture.lua("torch.texture = nil return tostring(torch.texture)"), "nil");
    EXPECT_EQ(fixture.lua("return tostring(torch:affects(1, 0)) .. ' ' .. tostring(torch:affects(4, 0)) .. ' ' .. tostring(torch:affects(1, 5))"), "true false false");

    // A white point light adds its falloff over the ambient color, and light behind an occluder of its shadow mask is blocked.
    fixture.runLua("lamp = lighting2d.newLight({x = 0, y = 0, radius = 100, shadows = true})");
    EXPECT_EQ(fixture.lua("return lamp:strengthAt(0, 0) .. ' ' .. lamp:strengthAt(200, 0)"), "1.0 0.0");
    EXPECT_EQ(fixture.lua("local c = lamp:apply('#FF202020', 0, 0) return string.format('%.3f', c.r)"), "1.125");
    EXPECT_EQ(fixture.lua("local c = lighting2d.illuminate('#FF000000', {lamp, {x = 0, y = 0, radius = 100, intensity = 0.5, blend = 'subtract'}}, 0, 0) return c.g"), "0.5");
    EXPECT_EQ(fixture.lua("local c = lighting2d.illuminate('#FF000000', {lamp}, 0, 0, {lightMask = 2}) return c.b"), "0.0");
    fixture.runLua("wall = lighting2d.newOccluder({points = {50, -20, 50, 20}, closed = false})");
    EXPECT_EQ(fixture.lua("return tostring(lamp:shadowedAt(80, 0, {wall})) .. ' ' .. tostring(lamp:shadowedAt(20, 0, {wall})) .. ' ' .. tostring(lamp:shadowedAt(80, 0, {{points = {{50, -20}, {50, 20}}, closed = false, mask = 2}}))"), "true false false");
    EXPECT_NE(fixture.lua("lamp:shadowedAt(80, 0, {{points = {}, closed = false}})").find("An occluder needs at least 2 points, and 3 when it is closed."), std::string::npos);

    // The falloff is the curve of lights without a texture, from 1 at the center to 0 at the radius.
    EXPECT_EQ(fixture.lua("return lighting2d.falloff(0) .. ' ' .. lighting2d.falloff(1) .. ' ' .. tostring(lighting2d.falloff(0.5) == lamp:strengthAt(50, 0))"), "1.0 0.0 true");

    EXPECT_NE(fixture.lua("lighting2d.newLight({kind = 'spot'})").find("Unknown option \"kind\""), std::string::npos);
    EXPECT_NE(fixture.lua("lighting2d.newLight({type = 'area'})").find("unknown value 'area'"), std::string::npos);
    EXPECT_NE(fixture.lua("torch.itemMask = 300").find("integer out of range"), std::string::npos);
    EXPECT_NE(fixture.lua("lighting2d.illuminate('#FF000000', {}, 0, 0, {mask = 1})").find("Unknown option \"mask\""), std::string::npos);
}

TEST(Lighting2DLuaTest, BuildsOccludersFromTablesBodiesAndMaps) {
    test::EngineFixture fixture;
    fixture.runLua(R"(
        lighting2d = require('haylen.lighting2d')
        physics2d = require('haylen.physics2d')
        wall = lighting2d.newOccluder({points = {{0, 0}, {10, 0}, {10, 10}}, cull = 'clockwise', mask = 3, x = 5, y = 6, rotation = 0, scaleX = 2, scaleY = 1})
        world = physics2d.newWorld({gravity = {0, 0}})
        crate = world:createBody({x = 100, y = 50})
        crate:addBox(20, 20)
    )");

    EXPECT_EQ(fixture.lua("return #wall.points .. ' ' .. tostring(wall.closed) .. ' ' .. wall.cull .. ' ' .. wall.mask .. ' ' .. wall.x .. ' ' .. wall.scaleX"), "3 true clockwise 3 5.0 2.0");
    EXPECT_EQ(fixture.lua("local points = wall:worldPoints() return points[2].x .. ' ' .. points[2].y"), "25.0 6.0");
    EXPECT_EQ(fixture.lua("wall.points = {1, 2, 3, 4} wall.closed = false return #wall.points .. ' ' .. wall.points[2].y"), "2 4.0");
    EXPECT_EQ(fixture.lua("local list = lighting2d.occludersFromBody(world, crate) return #list .. ' ' .. #list[1].points .. ' ' .. list[1].x .. ' ' .. tostring(list[1].closed)"), "1 4 100.0 true");

    EXPECT_NE(fixture.lua("wall.points = {1, 2, 3}").find("two numbers per point"), std::string::npos);
    EXPECT_NE(fixture.lua("lighting2d.newOccluder({cull = 'both'})").find("unknown value 'both'"), std::string::npos);
    EXPECT_NE(fixture.lua("lighting2d.newOccluder({point = {}})").find("Unknown option \"point\""), std::string::npos);
}

} // namespace haylen
