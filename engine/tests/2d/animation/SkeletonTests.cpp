#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/2d/animation/Skeleton.hpp"
#include "haylen/2d/animation/SkeletonData.hpp"
#include "haylen/2d/animation/SpriteAtlas.hpp"
#include "haylen/2d/animation/StateGraph.hpp"
#include "haylen/2d/animation/StateGraphPlayer.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "support/EngineFixture.hpp"
#include "support/FrameRenderer.hpp"
#include "support/TestFiles.hpp"

namespace haylen::animation2d {

class SkeletonTest : public ::testing::Test {
  protected:
    static constexpr const char* kAtlas = R"({
        "frames": {
            "body": {"frame": {"x": 0, "y": 0, "w": 32, "h": 64}},
            "arm": {"frame": {"x": 32, "y": 0, "w": 32, "h": 8}},
            "blade": {"frame": {"x": 32, "y": 8, "w": 32, "h": 8}},
            "cape": {"frame": {"x": 0, "y": 64, "w": 32, "h": 32}}
        },
        "meta": {"image": "hero.png"}
    })";

    // A body with an upper arm and a forearm, a sword skin, a cape mesh, an aim constraint and three animations.
    static constexpr const char* kSkeleton = R"({
        "atlas": "hero.json",
        "bones": [
            {"name": "root"},
            {"name": "body", "parent": "root", "y": -32, "length": 32},
            {"name": "upper", "parent": "body", "x": 10, "y": -20, "rotation": 90, "length": 30},
            {"name": "lower", "parent": "upper", "x": 30, "length": 30},
            {"name": "aim", "parent": "root", "x": 100}
        ],
        "slots": [
            {"name": "body", "bone": "body", "attachment": "body"},
            {"name": "cape", "bone": "body", "attachment": "cape"},
            {"name": "hand", "bone": "lower", "attachment": "fist"},
            {"name": "tip", "bone": "lower", "attachment": "tip"}
        ],
        "skins": {
            "default": {
                "body": {"body": {}},
                "cape": {"cape": {"type": "mesh", "vertices": [0, 0, 10, 0, 10, 10, 0, 10], "uvs": [0, 0, 1, 0, 1, 1, 0, 1], "triangles": [0, 1, 2, 0, 2, 3]}},
                "hand": {"fist": {"frame": "arm"}},
                "tip": {"tip": {"type": "point", "x": 30}}
            },
            "sword": {"hand": {"fist": {"frame": "blade"}}}
        },
        "ik": [{"name": "reach", "bones": ["upper", "lower"], "target": "aim", "mix": 0}],
        "animations": {
            "wave": {"bones": {"upper": {"rotate": [{"time": 0, "value": 0}, {"time": 1, "value": -90}]}}, "events": [{"time": 0.5, "name": "step", "text": "left", "number": 2}]},
            "lift": {"bones": {"body": {"translate": [{"time": 0, "x": 0, "y": 0}, {"time": 1, "x": 0, "y": -10}]}}},
            "flap": {"deform": {"cape": {"cape": [{"time": 0, "offsets": [0, 0, 0, 0, 0, 0, 0, 0]}, {"time": 1, "offsets": [0, 0, 4, 0, 4, 0, 0, 0]}]}}, "slots": {"hand": {"attachment": [{"time": 0, "name": "fist"}, {"time": 0.5, "name": ""}]}}}
        }
    })";

    [[nodiscard]] static std::shared_ptr<SkeletonData> makeData(test::EngineFixture& fixture, const char* document = kSkeleton) {
        auto atlas = std::make_shared<SpriteAtlas>(SpriteAtlas::parse(nlohmann::ordered_json::parse(kAtlas), fixture.engine().getGraphics().createTexture(graphics::Image(64, 96))));
        return std::make_shared<SkeletonData>(SkeletonData::parse(core::Json::parse(document), atlas));
    }

    static void expectNear(math::Vec2 actual, math::Vec2 expected) {
        EXPECT_NEAR(actual.x, expected.x, 1.0e-3F);
        EXPECT_NEAR(actual.y, expected.y, 1.0e-3F);
    }
};

class SkeletonLuaTest : public SkeletonTest {};

TEST_F(SkeletonTest, ComposesBoneTransformsAndPlaysTracks) {
    test::EngineFixture fixture;
    Skeleton skeleton(makeData(fixture));
    skeleton.position = {200.0F, 300.0F};
    skeleton.pose();

    // The upper arm turns a quarter down from the body, so the forearm starts 30 below the shoulder.
    const std::size_t lower = *skeleton.findBone("lower");
    expectNear(skeleton.getBoneWorld(*skeleton.findBone("upper")).getTranslation(), {210.0F, 248.0F});
    expectNear(skeleton.getBoneWorld(lower).getTranslation(), {210.0F, 278.0F});
    expectNear(*skeleton.getPoint("tip"), {210.0F, 308.0F});

    // Halfway through the wave the arm turned back by 45 degrees, and the event fires once per pass.
    std::vector<std::string> heard;
    skeleton.eventFired.connect([&heard](const Skeleton::Event& event) { heard.push_back(std::string(event.name) + ":" + std::string(event.text) + ":" + std::to_string(event.track)); });
    skeleton.play("wave", {.track = 0, .loop = true});
    skeleton.update(0.5F);
    EXPECT_NEAR(skeleton.getBonePose(*skeleton.findBone("upper")).rotation, std::numbers::pi_v<float> * 0.25F, 1.0e-4F);
    skeleton.update(0.75F);
    skeleton.update(2.0F);
    EXPECT_EQ(heard, (std::vector<std::string>{"step:left:0", "step:left:0", "step:left:0"}));

    // A second track mixes on top, by its weight.
    skeleton.play("lift", {.track = 1, .loop = false});
    skeleton.setTrackWeight(1, 0.5F);
    skeleton.update(1.0F);
    expectNear(skeleton.getBonePose(*skeleton.findBone("body")).position, {0.0F, -37.0F});
    EXPECT_EQ(skeleton.getTrackAnimation(1), "lift");
}

TEST_F(SkeletonTest, CrossfadesBetweenAnimations) {
    test::EngineFixture fixture;
    Skeleton skeleton(makeData(fixture));
    skeleton.play("wave", {.track = 0, .loop = false});
    skeleton.update(1.0F);
    const std::size_t upper = *skeleton.findBone("upper");
    EXPECT_NEAR(skeleton.getBonePose(upper).rotation, 0.0F, 1.0e-4F);

    // The lift starts while the wave fades out over a second, so the arm returns to its setup halfway.
    skeleton.play("lift", {.track = 0, .loop = false, .fade = 1.0F});
    skeleton.update(0.5F);
    EXPECT_NEAR(skeleton.getBonePose(upper).rotation, std::numbers::pi_v<float> * 0.25F, 1.0e-3F);
    skeleton.update(0.6F);
    EXPECT_NEAR(skeleton.getBonePose(upper).rotation, std::numbers::pi_v<float> * 0.5F, 1.0e-3F);
}

TEST_F(SkeletonTest, ReachesTargetsWithTwoBones) {
    test::EngineFixture fixture;
    Skeleton skeleton(makeData(fixture));
    skeleton.setIkMix("reach", 1.0F);
    skeleton.setIkTarget("reach", {40.0F, -52.0F});
    skeleton.pose();

    // The shoulder sits at (10, -52) and the arm is 60 long, so a target 30 away bends the elbow while the end of the forearm lands on it.
    const std::size_t lower = *skeleton.findBone("lower");
    const math::Vec2 end = skeleton.getBoneWorld(lower).apply({30.0F, 0.0F});
    expectNear(end, {40.0F, -52.0F});
    const float bentUp = skeleton.getBoneWorld(lower).getTranslation().y;

    skeleton.setIkTarget("reach", {40.0F, -52.0F});
    Skeleton other(makeData(fixture, R"({"atlas": "a", "bones": [{"name": "root"}, {"name": "body", "parent": "root", "y": -32}, {"name": "upper", "parent": "body", "x": 10, "y": -20, "rotation": 90, "length": 30}, {"name": "lower", "parent": "upper", "x": 30, "length": 30}, {"name": "aim", "parent": "root"}], "ik": [{"name": "reach", "bones": ["upper", "lower"], "target": "aim", "bendPositive": false}]})"));
    other.setIkTarget("reach", {40.0F, -52.0F});
    other.pose();
    expectNear(other.getBoneWorld(*other.findBone("lower")).apply({30.0F, 0.0F}), {40.0F, -52.0F});
    EXPECT_NEAR(other.getBoneWorld(*other.findBone("lower")).getTranslation().y, -52.0F - (bentUp + 52.0F), 1.0e-3F);

    // A target out of reach stretches the arm straight toward it.
    skeleton.setIkTarget("reach", {200.0F, -52.0F});
    skeleton.pose();
    expectNear(skeleton.getBoneWorld(lower).apply({30.0F, 0.0F}), {70.0F, -52.0F});
}

TEST_F(SkeletonTest, SwapsSkinsAttachmentsAndDeformsMeshes) {
    test::EngineFixture fixture;
    Skeleton skeleton(makeData(fixture));
    const math::Rect before = skeleton.getBounds();
    skeleton.play("flap", {.track = 0, .loop = false});
    skeleton.update(1.0F);
    const math::Rect after = skeleton.getBounds();

    // The cape grows to the right by its deform, and the fist went away at half time.
    EXPECT_GE(after.getRight(), before.getRight());
    EXPECT_EQ(skeleton.getTrackAnimation(0), "flap");
    skeleton.stop(0);
    skeleton.setSkin("sword");
    skeleton.setAttachment("hand", "fist");
    EXPECT_EQ(skeleton.getSkin(), "sword");
    EXPECT_THROW(skeleton.setAttachment("hand", "shield"), std::invalid_argument);
    EXPECT_THROW(skeleton.setSkin("armor"), std::invalid_argument);

    // The body, the cape and the fist draw as one mesh of 4, 4 and 4 vertices.
    // clang-format off
    const graphics2d::Renderer::Stats stats = test::FrameRenderer::renderOnce(fixture, [&skeleton](core::Engine& engine) {
        engine.getRenderer2D().beginScreen();
        skeleton.pose();
        skeleton.draw(engine.getRenderer2D());
    });
    // clang-format on
    EXPECT_EQ(stats.drawCalls, 1U);
    EXPECT_EQ(stats.vertices, 12U);
    EXPECT_EQ(stats.indices, 18U);
}

TEST_F(SkeletonTest, DrivesTracksFromStateGraphs) {
    test::EngineFixture fixture;
    auto skeleton = std::make_shared<Skeleton>(makeData(fixture));
    const auto graph = std::make_shared<StateGraph>(StateGraph::parse(core::Json::parse(R"({"parameters": {"lifting": false}, "initial": "wave", "states": {"wave": {"animation": "wave"}, "lift": {"animation": "lift"}}, "transitions": [{"from": "wave", "to": "lift", "when": "lifting", "fade": 0.25}]})")));
    StateGraphPlayer player(graph, StateGraphPlayer::SkeletonTrack{.skeleton = skeleton, .track = 1});
    EXPECT_EQ(skeleton->getTrackAnimation(1), "wave");
    player.setBoolean("lifting", true);
    player.update(0.0F);
    EXPECT_EQ(skeleton->getTrackAnimation(1), "lift");
}

TEST_F(SkeletonTest, ExplainsBadFiles) {
    test::EngineFixture fixture;
    // clang-format off
    const auto error = [&fixture](const char* document) {
        try {
            (void)makeData(fixture, document);
        } catch (const std::invalid_argument& failure) {
            return std::string(failure.what());
        }
        return std::string();
    };
    // clang-format on
    EXPECT_EQ(error(R"({"bones": [{"name": "root"}, {"name": "arm", "parent": "torso"}]})"), "The bone \"arm\" names the unknown bone \"torso\".");
    EXPECT_EQ(error(R"({"bones": [{"name": "root"}], "slots": [{"name": "a", "bone": "root", "attachment": "x"}]})"), "The slot \"a\" starts with the attachment \"x\", which no skin has.");
    EXPECT_EQ(error(R"({"bones": [{"name": "root"}], "animations": {"walk": {"bones": {"root": {"spin": []}}}}})"), "The bone \"root\" in the animation \"walk\" has the unknown timeline \"spin\". Bones take \"rotate\", \"translate\" and \"scale\".");
    EXPECT_EQ(error(R"({"bones": [{"name": "root"}], "animations": {"walk": {"bones": {"root": {"rotate": [{"time": 1}, {"time": 0}]}}}}})"), "The keys of the rotate of the bone \"root\" in the animation \"walk\" must follow each other in time.");
    EXPECT_EQ(error(R"({"bones": [{"name": "root"}], "slots": [{"name": "a", "bone": "root"}], "skins": {"default": {"a": {"x": {"frame": "missing"}}}}})"), "The frame \"missing\" of the attachment \"x\" of the slot \"a\" does not exist in the atlas of the skeleton.");
    EXPECT_EQ(error(R"({"bones": []})"), "A skeleton needs at least one bone.");
}

TEST_F(SkeletonLuaTest, LoadsPlaysAndDrawsSkeletonsFromLua) {
    const std::vector<std::uint8_t> image = test::TestFiles::pngImage(64, 96, 0xFFFFFFFFU);
    test::EngineFixture fixture({{"content/hero.json", kAtlas}, {"content/hero.png", std::string(image.begin(), image.end())}, {"content/hero.skeleton", kSkeleton}});
    // clang-format off
    EXPECT_EQ(fixture.lua(R"(
        animation2d = require('haylen.animation2d')
        assets = require('haylen.assets')
        local data = assets.load('hero.skeleton')
        hero = animation2d.newSkeleton(data, {x = 200, y = 300, skin = 'sword'})
        heard = {}
        hero:on('event', function(name, track, animation, text, number) heard[#heard + 1] = name .. track .. animation .. text .. number end, {owner = hero})
        hero:play('wave', {track = 1})
        hero:update(0.6)
        local bone = hero:bone('lower')
        local x, y = hero:point('tip')
        return #data.bones .. ' ' .. table.concat(data.animations, ',') .. ' ' .. hero.skin .. ' ' .. string.format('%.1f %.1f', bone.worldX, x) .. ' ' .. table.concat(heard, ',') .. ' ' .. hero:trackAnimation(1)
    )"), "5 flap,lift,wave sword 234.3 258.5 step1waveleft2.0 wave");
    // clang-format on
    EXPECT_EQ(fixture.lua("hero:crossFade('lift', 0.2, {track = 1, loop = false}) hero:setIkTarget('reach', 220, 250) hero:setIkMix('reach', 1) hero:update(0.1) local bounds = hero:bounds() return tostring(bounds.width > 0) .. ' ' .. hero.data:duration('lift')"), "true 1.0");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(hero.play, hero, 'fly') return message:match('The skeleton.*')"), "The skeleton has no animation \"fly\".");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(hero.on, hero, 'step', function() end) return message:match('Unknown.*')"), "Unknown skeleton event \"step\". Skeletons report \"event\".");
}

} // namespace haylen::animation2d
