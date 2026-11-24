#include <gtest/gtest.h>

#include <memory>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/2d/animation/Skeleton.hpp"
#include "haylen/2d/animation/SkeletonData.hpp"
#include "haylen/2d/animation/SpriteAtlas.hpp"
#include "haylen/2d/animation/StateGraph.hpp"
#include "haylen/2d/animation/StateGraphPlayer.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Image.hpp"
#include "support/EngineFixture.hpp"
#include "support/FrameRenderer.hpp"
#include "support/SkeletonFiles.hpp"
#include "support/VectorExpectations.hpp"

namespace haylen::animation2d {

class SkeletonTest : public ::testing::Test {
  protected:
    [[nodiscard]] static std::shared_ptr<SkeletonData> makeData(test::EngineFixture& fixture, const char* document = test::SkeletonFiles::kSkeleton) {
        auto atlas = std::make_shared<SpriteAtlas>(SpriteAtlas::parse(nlohmann::ordered_json::parse(test::SkeletonFiles::kAtlas), fixture.engine().getGraphics().createTexture(haylen::graphics::Image(64, 96))));
        return std::make_shared<SkeletonData>(SkeletonData::parse(haylen::core::Json::parse(document), atlas));
    }

    [[nodiscard]] static std::size_t findBone(const Skeleton& skeleton, std::string_view name) {
        const std::optional<std::size_t> bone = skeleton.findBone(name);
        if (!bone) {
            throw std::runtime_error("The skeleton has no bone \"" + std::string(name) + "\".");
        }

        return *bone;
    }
};

TEST_F(SkeletonTest, ComposesBoneTransformsAndPlaysTracks) {
    test::EngineFixture fixture;
    Skeleton skeleton(makeData(fixture));
    skeleton.position = {200.0F, 300.0F};
    skeleton.pose();

    // The upper arm turns a quarter down from the body, so the forearm starts 30 below the shoulder.
    const std::size_t lower = findBone(skeleton, "lower");
    test::VectorExpectations::expectNear(skeleton.getBoneWorld(findBone(skeleton, "upper")).getTranslation(), {210.0F, 248.0F});
    test::VectorExpectations::expectNear(skeleton.getBoneWorld(lower).getTranslation(), {210.0F, 278.0F});
    const std::optional<haylen::math::Vec2> tip = skeleton.getPoint("tip");
    ASSERT_TRUE(tip.has_value());
    test::VectorExpectations::expectNear(*tip, {210.0F, 308.0F});

    // Halfway through the wave the arm turned back by 45 degrees, and the event fires once per pass.
    std::vector<std::string> heard;
    skeleton.eventFired.connect([&heard](const Skeleton::Event& event) { heard.push_back(std::string(event.name) + ":" + std::string(event.text) + ":" + std::to_string(event.track)); });
    skeleton.play("wave", {.track = 0, .loop = true});
    skeleton.update(0.5F);
    EXPECT_NEAR(skeleton.getBonePose(findBone(skeleton, "upper")).rotation, std::numbers::pi_v<float> * 0.25F, 1.0e-4F);
    skeleton.update(0.75F);
    skeleton.update(2.0F);
    EXPECT_EQ(heard, (std::vector<std::string>{"step:left:0", "step:left:0", "step:left:0"}));

    // A second track mixes on top, by its weight.
    skeleton.play("lift", {.track = 1, .loop = false});
    skeleton.setTrackWeight(1, 0.5F);
    skeleton.update(1.0F);
    test::VectorExpectations::expectNear(skeleton.getBonePose(findBone(skeleton, "body")).position, {0.0F, -37.0F});
    EXPECT_EQ(skeleton.getTrackAnimation(1), "lift");
}

TEST_F(SkeletonTest, FiresEventsSafelyWhenListenersUpdateAndLongStepsPass) {
    test::EngineFixture fixture;
    Skeleton skeleton(makeData(fixture));
    skeleton.play("wave", {.track = 0, .loop = true});

    // A listener that updates the skeleton again hears its own events without losing the ones still waiting.
    std::vector<float> heard;
    bool nested = false;
    // clang-format off
    skeleton.eventFired.connect([&](const Skeleton::Event& event) {
        heard.push_back(event.number);

        if (!nested) {
            nested = true;
            skeleton.update(2.0F);
        }
    });
    // clang-format on
    skeleton.update(1.5F);
    EXPECT_EQ(heard.size(), 4U);

    // A step of a billion seconds reports the events of its last passes and ends.
    heard.clear();
    skeleton.update(1.0e9F);
    EXPECT_GE(heard.size(), 1U);
    EXPECT_LE(heard.size(), 3U);

    EXPECT_THROW(skeleton.play("wave", {.track = Skeleton::kMaxTracks}), std::out_of_range);
    EXPECT_THROW(skeleton.setTrackWeight(Skeleton::kMaxTracks, 0.5F), std::out_of_range);
}

TEST_F(SkeletonTest, CrossfadesBetweenAnimations) {
    test::EngineFixture fixture;
    Skeleton skeleton(makeData(fixture));
    skeleton.play("wave", {.track = 0, .loop = false});
    skeleton.update(1.0F);
    const std::size_t upper = findBone(skeleton, "upper");
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
    const std::size_t lower = findBone(skeleton, "lower");
    const haylen::math::Vec2 end = skeleton.getBoneWorld(lower).apply({30.0F, 0.0F});
    test::VectorExpectations::expectNear(end, {40.0F, -52.0F});
    const float bentUp = skeleton.getBoneWorld(lower).getTranslation().y;

    skeleton.setIkTarget("reach", {40.0F, -52.0F});
    Skeleton other(makeData(fixture, R"({"atlas": "a", "bones": [{"name": "root"}, {"name": "body", "parent": "root", "y": -32}, {"name": "upper", "parent": "body", "x": 10, "y": -20, "rotation": 90, "length": 30}, {"name": "lower", "parent": "upper", "x": 30, "length": 30}, {"name": "aim", "parent": "root"}], "ik": [{"name": "reach", "bones": ["upper", "lower"], "target": "aim", "bendPositive": false}]})"));
    other.setIkTarget("reach", {40.0F, -52.0F});
    other.pose();
    test::VectorExpectations::expectNear(other.getBoneWorld(findBone(other, "lower")).apply({30.0F, 0.0F}), {40.0F, -52.0F});
    EXPECT_NEAR(other.getBoneWorld(findBone(other, "lower")).getTranslation().y, -52.0F - (bentUp + 52.0F), 1.0e-3F);

    // A target out of reach stretches the arm straight toward it.
    skeleton.setIkTarget("reach", {200.0F, -52.0F});
    skeleton.pose();
    test::VectorExpectations::expectNear(skeleton.getBoneWorld(lower).apply({30.0F, 0.0F}), {70.0F, -52.0F});
}

// A bone whose parent is scaled to nothing has no place inside the parent for the target of a transform constraint, so it keeps its position and the pose goes on.
TEST_F(SkeletonTest, KeepsTheBonesOfAParentScaledToNothingWhereTheyAre) {
    test::EngineFixture fixture;
    Skeleton skeleton(makeData(fixture, R"({"atlas": "a", "bones": [{"name": "root"}, {"name": "hidden", "parent": "root", "scaleX": 0, "scaleY": 0}, {"name": "follower", "parent": "hidden", "x": 5}, {"name": "aim", "parent": "root", "x": 40, "y": 10}], "transform": [{"name": "stick", "target": "aim", "bones": ["follower"], "rotateMix": 0, "translateMix": 1}]})"));
    skeleton.pose();
    const std::optional<std::size_t> follower = skeleton.findBone("follower");
    ASSERT_TRUE(follower.has_value());
    test::VectorExpectations::expectNear(skeleton.getBoneWorld(*follower).getTranslation(), {0.0F, 0.0F});
}

TEST_F(SkeletonTest, SwapsSkinsAttachmentsAndDeformsMeshes) {
    test::EngineFixture fixture;
    Skeleton skeleton(makeData(fixture));
    const haylen::math::Rect before = skeleton.getBounds();
    skeleton.play("flap", {.track = 0, .loop = false});
    skeleton.update(1.0F);
    const haylen::math::Rect after = skeleton.getBounds();

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
    const haylen::graphics2d::Renderer::Stats stats = test::FrameRenderer::renderOnce(fixture, [&skeleton](haylen::core::Engine& engine) {
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
    const auto graph = std::make_shared<StateGraph>(StateGraph::parse(haylen::core::Json::parse(R"({"parameters": {"lifting": false}, "initial": "wave", "states": {"wave": {"animation": "wave"}, "lift": {"animation": "lift"}}, "transitions": [{"from": "wave", "to": "lift", "when": "lifting", "fade": 0.25}]})")));
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

    for (const char* triangles : {"[-1, 1, 2]", "[0, 1, 2.5]", "[0, 1, 4294967296]"}) {
        const std::string mesh = R"({"bones": [{"name": "root"}], "slots": [{"name": "cape", "bone": "root"}], "skins": {"default": {"cape": {"cape": {"type": "mesh", "vertices": [0, 0, 10, 0, 10, 10], "uvs": [0, 0, 1, 0, 1, 1], "triangles": )" + std::string(triangles) + "}}}}}";
        EXPECT_NE(error(mesh.c_str()).find("The key \"triangles\" of "), std::string::npos) << triangles;
        EXPECT_NE(error(mesh.c_str()).find(" must be a list of whole numbers from 0 to 4294967295."), std::string::npos) << triangles;
    }

    EXPECT_EQ(error(R"({"bones": []})"), "A skeleton needs at least one bone.");
    EXPECT_EQ(error(R"({"bones": [{"name": "root"}], "slots": [{"name": "a", "bone": "root"}, {"name": "b", "bone": "root"}], "animations": {"swap": {"drawOrder": [{"order": ["a", "a"]}]}}})"), "Every draw order key in the animation \"swap\" lists every slot once.");
    EXPECT_EQ(error(R"({"bones": [{"name": "root"}], "slots": [{"name": "a", "bone": "root"}], "animations": {"swap": {"drawOrder": [{"time": 1, "order": ["a"]}, {"time": 0, "order": ["a"]}]}}})"), "The draw order keys in the animation \"swap\" must follow each other in time.");
    EXPECT_EQ(error(R"({"bones": [{"name": "root"}], "slots": [{"name": "glow", "bone": "root", "blend": "softLight"}]})"), "The slot \"glow\" has the blend mode \"softLight\", which mixes with what lies below and blends groups alone.");
}

} // namespace haylen::animation2d
