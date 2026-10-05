#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/2d/animation/Animation.hpp"
#include "haylen/2d/animation/Animator.hpp"
#include "haylen/2d/animation/SpriteAtlas.hpp"
#include "haylen/2d/graphics/Sprite.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/math/Insets.hpp"
#include "support/EngineFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::animation2d {

class AnimationTest : public ::testing::Test {
  protected:
    [[nodiscard]] static Animation makeStrip(graphics::Texture texture, Animation::Loop loop) {
        return Animation::fromGrid(std::move(texture), {.frameSize = {16.0F, 16.0F}, .cells = {}, .framesPerSecond = 10.0F, .loop = loop});
    }
};

class AnimatorTest : public AnimationTest {};

class SpriteAtlasTest : public ::testing::Test {
  protected:
    static constexpr const char* kAsepriteAtlas = R"({
        "frames": [
            {"filename": "hero 0", "frame": {"x": 0, "y": 0, "w": 8, "h": 16}, "rotated": false, "trimmed": true, "spriteSourceSize": {"x": 4, "y": 0, "w": 8, "h": 16}, "sourceSize": {"w": 16, "h": 16}, "duration": 100},
            {"filename": "hero 1", "frame": {"x": 8, "y": 0, "w": 16, "h": 16}, "duration": 200},
            {"filename": "hero 2", "frame": {"x": 24, "y": 0, "w": 16, "h": 16}, "duration": 100},
            {"filename": "panel", "frame": {"x": 0, "y": 16, "w": 32, "h": 16}, "duration": 100}
        ],
        "meta": {
            "image": "hero.png",
            "frameTags": [
                {"name": "walk", "from": 0, "to": 2, "direction": "forward"},
                {"name": "back", "from": 0, "to": 2, "direction": "reverse"},
                {"name": "bounce", "from": 0, "to": 2, "direction": "pingpong"},
                {"name": "hit", "from": 1, "to": 2, "direction": "forward", "repeat": "1"}
            ],
            "slices": [
                {"name": "panel", "keys": [{"frame": 3, "bounds": {"x": 0, "y": 0, "w": 32, "h": 16}, "center": {"x": 4, "y": 4, "w": 24, "h": 8}}]},
                {"name": "marker", "keys": [{"frame": 0, "bounds": {"x": 0, "y": 0, "w": 2, "h": 2}}]}
            ]
        }
    })";
};

class Animation2DLuaTest : public SpriteAtlasTest {};

TEST_F(AnimationTest, CutsGridsAndPicksFramesOverTime) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(64, 32));

    const animation2d::Animation all = makeStrip(texture, animation2d::Animation::Loop::Loop);
    ASSERT_EQ(all.frames.size(), 8U);
    EXPECT_EQ(all.frames[5].source, (math::Rect{16.0F, 16.0F, 16.0F, 16.0F}));
    EXPECT_FLOAT_EQ(all.getDuration(), 0.8F);
    EXPECT_EQ(all.frameAt(0.0F), 0U);
    EXPECT_EQ(all.frameAt(0.25F), 2U);
    EXPECT_EQ(all.frameAt(0.85F), 0U);
    EXPECT_EQ(all.frameAt(-1.0F), 0U);

    const animation2d::Animation spaced = animation2d::Animation::fromGrid(texture, {.frameSize = {14.0F, 14.0F}, .cells = {1, 3}, .framesPerSecond = 5.0F, .loop = animation2d::Animation::Loop::Once, .margin = {1.0F, 1.0F}, .spacing = {2.0F, 2.0F}});
    ASSERT_EQ(spaced.frames.size(), 2U);
    EXPECT_EQ(spaced.frames[0].source, (math::Rect{17.0F, 1.0F, 14.0F, 14.0F}));
    EXPECT_EQ(spaced.frames[1].source, (math::Rect{49.0F, 1.0F, 14.0F, 14.0F}));
    EXPECT_EQ(spaced.frameAt(10.0F), 1U);

    animation2d::Animation bounce = animation2d::Animation::fromGrid(texture, {.frameSize = {16.0F, 16.0F}, .cells = {0, 1, 2, 3}, .framesPerSecond = 10.0F, .loop = animation2d::Animation::Loop::PingPong});
    std::vector<std::size_t> sequence;
    for (int step = 0; step < 8; ++step) {
        sequence.push_back(bounce.frameAt(static_cast<float>(step) * 0.1F + 0.05F));
    }
    EXPECT_EQ(sequence, (std::vector<std::size_t>{0, 1, 2, 3, 2, 1, 0, 1}));

    bounce.frames.resize(1);
    EXPECT_EQ(bounce.frameAt(5.0F), 0U);

    EXPECT_THROW((void)animation2d::Animation::fromGrid(graphics::Texture{}, {.frameSize = {16.0F, 16.0F}}), std::invalid_argument);
    EXPECT_THROW((void)animation2d::Animation::fromGrid(texture, {.frameSize = {128.0F, 16.0F}}), std::invalid_argument);
    EXPECT_THROW((void)animation2d::Animation::fromGrid(texture, {.frameSize = {16.0F, 16.0F}, .cells = {8}}), std::out_of_range);
    EXPECT_EQ(animation2d::Animation::loopFromName("pingPong"), animation2d::Animation::Loop::PingPong);
    EXPECT_FALSE(animation2d::Animation::loopFromName("forever").has_value());
    EXPECT_EQ(animation2d::Animation::loopName(animation2d::Animation::Loop::Once), "once");
}

TEST_F(AnimatorTest, PlaysAnimationsAndReportsProgress) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(64, 16));

    animation2d::Animator animator;
    animator.add("run", makeStrip(texture, animation2d::Animation::Loop::Loop));
    animator.add("attack", makeStrip(texture, animation2d::Animation::Loop::Once));
    EXPECT_TRUE(animator.has("run"));
    EXPECT_FALSE(animator.isFinished());

    std::vector<std::string> events;
    animator.onFrame = [&](std::string_view name, std::size_t frame) { events.push_back(std::string(name) + ":" + std::to_string(frame)); };
    animator.onFinish = [&](std::string_view name) { events.push_back(std::string(name) + ":done"); };

    animator.update(1.0F);
    animator.play("run");
    animator.update(0.15F);
    EXPECT_EQ(animator.getFrame(), 1U);
    animator.play("run");
    EXPECT_EQ(animator.getFrame(), 1U);
    animator.play("run", true);
    EXPECT_EQ(animator.getFrame(), 0U);

    animator.play("attack");
    animator.speed = 2.0F;
    animator.update(0.1F);
    animator.update(1.0F);
    EXPECT_TRUE(animator.isFinished());
    EXPECT_FALSE(animator.isPlaying());
    EXPECT_EQ(events, (std::vector<std::string>{"run:1", "attack:2", "attack:3", "attack:done"}));

    graphics2d::Sprite sprite;
    animator.apply(sprite);
    EXPECT_EQ(sprite.texture, texture);
    EXPECT_EQ(sprite.source, (math::Rect{48.0F, 0.0F, 16.0F, 16.0F}));
    EXPECT_EQ(sprite.pivot, math::Vec2(0.5F, 0.5F));

    // Replacing the animation that plays finds its frame again among the new frames.
    animator.add("attack", animation2d::Animation::fromGrid(texture, {.frameSize = {16.0F, 16.0F}, .cells = {2}, .framesPerSecond = 10.0F, .loop = animation2d::Animation::Loop::Once}));
    EXPECT_EQ(animator.getFrame(), 0U);
    animator.apply(sprite);
    EXPECT_EQ(sprite.source, (math::Rect{32.0F, 0.0F, 16.0F, 16.0F}));

    animator.play("run");
    animator.stop();
    EXPECT_FALSE(animator.isPlaying());
    EXPECT_THROW(animator.play("fly"), std::invalid_argument);
    EXPECT_THROW(animator.add("", makeStrip(texture, animation2d::Animation::Loop::Loop)), std::invalid_argument);

    animation2d::Animator empty;
    graphics2d::Sprite untouched;
    empty.apply(untouched);
    EXPECT_FALSE(untouched.texture.isValid());
}

TEST_F(AnimatorTest, QueuesAnimationsAfterTheCurrentPass) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(64, 16));
    animation2d::Animator animator;
    animator.add("run", makeStrip(texture, animation2d::Animation::Loop::Loop));
    animator.add("attack", makeStrip(texture, animation2d::Animation::Loop::Once));
    animator.add("idle", makeStrip(texture, animation2d::Animation::Loop::PingPong));
    std::vector<std::string> finished;
    animator.onFinish = [&finished](std::string_view name) { finished.emplace_back(name); };

    animator.queue("attack");
    EXPECT_EQ(animator.getCurrent(), "attack");
    animator.queue("run");
    animator.queue("idle");
    EXPECT_EQ(animator.getQueuedCount(), 2U);

    animator.update(0.2F);
    EXPECT_EQ(animator.getCurrent(), "attack");
    animator.update(0.25F);
    EXPECT_EQ(finished, (std::vector<std::string>{"attack"}));
    EXPECT_EQ(animator.getCurrent(), "run");
    EXPECT_TRUE(animator.isPlaying());
    EXPECT_EQ(animator.getFrame(), 0U);
    EXPECT_EQ(animator.getQueuedCount(), 1U);

    animator.update(0.2F);
    EXPECT_EQ(animator.getCurrent(), "run");
    animator.update(0.25F);
    EXPECT_EQ(animator.getCurrent(), "idle");
    EXPECT_EQ(animator.getQueuedCount(), 0U);

    animator.queue("run");
    animator.play("attack");
    EXPECT_EQ(animator.getQueuedCount(), 0U);
    animator.queue("run");
    animator.clearQueue();
    EXPECT_EQ(animator.getQueuedCount(), 0U);
    EXPECT_THROW(animator.queue("fly"), std::invalid_argument);
}

TEST_F(AnimatorTest, QueuesAfterTheCycleInProgress) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(64, 16));
    animation2d::Animator animator;
    animator.add("run", makeStrip(texture, animation2d::Animation::Loop::Loop));
    animator.add("idle", makeStrip(texture, animation2d::Animation::Loop::PingPong));
    animator.add("attack", makeStrip(texture, animation2d::Animation::Loop::Once));
    EXPECT_FLOAT_EQ(animator.getAnimation("run").getCycleDuration(), 0.4F);
    EXPECT_FLOAT_EQ(animator.getAnimation("idle").getCycleDuration(), 0.6F);
    EXPECT_FLOAT_EQ(animator.getAnimation("attack").getCycleDuration(), 0.4F);

    // The loop is inside its third cycle when the queue call comes, so the switch waits for that cycle to end.
    animator.play("run");
    animator.update(1.0F);
    animator.queue("idle");
    animator.update(0.1F);
    EXPECT_EQ(animator.getCurrent(), "run");
    animator.update(0.15F);
    EXPECT_EQ(animator.getCurrent(), "idle");

    // A ping-pong cycle ends when the animation is back at its first frame.
    animator.queue("attack");
    animator.update(0.45F);
    EXPECT_EQ(animator.getCurrent(), "idle");
    animator.update(0.2F);
    EXPECT_EQ(animator.getCurrent(), "attack");

    animator.update(1.0F);
    EXPECT_TRUE(animator.isFinished());
    animator.queue("run");
    EXPECT_EQ(animator.getCurrent(), "run");
    EXPECT_TRUE(animator.isPlaying());
}

TEST_F(AnimatorTest, QueuesAFinishedOneShotAnimationAgainFromItsStart) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(64, 16));
    animation2d::Animator animator;
    animator.add("attack", makeStrip(texture, animation2d::Animation::Loop::Once));
    int finished = 0;
    animator.onFinish = [&finished](std::string_view) { ++finished; };

    animator.play("attack");
    animator.update(1.0F);
    ASSERT_TRUE(animator.isFinished());
    animator.queue("attack");
    EXPECT_TRUE(animator.isPlaying());
    EXPECT_FALSE(animator.isFinished());
    EXPECT_EQ(animator.getFrame(), 0U);
    animator.update(1.0F);
    EXPECT_EQ(finished, 2);
}

TEST_F(SpriteAtlasTest, ReadsAsepriteFramesTagsAndSlices) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(64, 32));
    const auto document = nlohmann::ordered_json::parse(kAsepriteAtlas);
    const animation2d::SpriteAtlas atlas = animation2d::SpriteAtlas::parse(document, texture);

    EXPECT_EQ(animation2d::SpriteAtlas::imagePath(document), "hero.png");
    EXPECT_EQ(atlas.getFrameNames(), (std::vector<std::string>{"hero 0", "hero 1", "hero 2", "panel"}));
    EXPECT_EQ(atlas.getFrame("hero 0").offset, math::Vec2(4.0F, 0.0F));
    EXPECT_EQ(atlas.getFrame("hero 0").originalSize, math::Vec2(16.0F, 16.0F));
    EXPECT_FLOAT_EQ(atlas.getFrame("hero 1").duration, 0.2F);
    EXPECT_TRUE(atlas.hasFrame("panel"));

    EXPECT_EQ(atlas.getAnimationNames(), (std::vector<std::string>{"back", "bounce", "hit", "walk"}));
    EXPECT_FLOAT_EQ(atlas.getAnimation("walk").getDuration(), 0.4F);
    EXPECT_EQ(atlas.getAnimation("back").frames.front().source.x, 24.0F);
    EXPECT_EQ(atlas.getAnimation("bounce").loop, animation2d::Animation::Loop::PingPong);
    EXPECT_EQ(atlas.getAnimation("hit").loop, animation2d::Animation::Loop::Once);
    EXPECT_TRUE(atlas.hasAnimation("walk"));

    EXPECT_EQ(atlas.getSliceNames(), (std::vector<std::string>{"panel"}));
    EXPECT_EQ(atlas.getSlice("panel").getBorders(), (math::Insets{.left = 4.0F, .top = 4.0F, .right = 4.0F, .bottom = 4.0F}));
    EXPECT_TRUE(atlas.hasSlice("panel"));
    EXPECT_FALSE(atlas.hasSlice("marker"));

    const animation2d::Animation custom = atlas.animationFromFrames({"hero 2", "hero 0"}, 4.0F, animation2d::Animation::Loop::Once);
    EXPECT_FLOAT_EQ(custom.getDuration(), 0.5F);

    animation2d::Animator animator;
    animator.add("walk", atlas.getAnimation("walk"));
    animator.play("walk");
    graphics2d::Sprite sprite;
    animator.apply(sprite);
    EXPECT_EQ(sprite.pivot, math::Vec2(0.5F, 0.5F));
    EXPECT_EQ(sprite.size, math::Vec2(8.0F, 16.0F));

    EXPECT_THROW((void)atlas.getFrame("missing"), std::invalid_argument);
    EXPECT_THROW((void)atlas.getAnimation("missing"), std::invalid_argument);
    EXPECT_THROW((void)atlas.getSlice("missing"), std::invalid_argument);
    EXPECT_THROW((void)atlas.animationFromFrames({}, 4.0F), std::invalid_argument);
    EXPECT_THROW((void)animation2d::SpriteAtlas::parse(document, graphics::Texture{}), std::invalid_argument);
}

// Slice bounds are relative to the untrimmed canvas of their frame, so a trimmed frame moves its slice by the pixels it dropped.
TEST_F(SpriteAtlasTest, PlacesTheSlicesOfTrimmedFramesInTheSheet) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(64, 32));
    // clang-format off
    const auto trimmed = [](const char* bounds) {
        return nlohmann::ordered_json::parse(std::string(R"({
            "frames": [{"filename": "panel", "frame": {"x": 40, "y": 10, "w": 20, "h": 12}, "trimmed": true, "spriteSourceSize": {"x": 2, "y": 3, "w": 20, "h": 12}, "sourceSize": {"w": 24, "h": 18}}],
            "meta": {"image": "ui.png", "slices": [{"name": "panel", "keys": [{"frame": 0, "bounds": )") + bounds + R"(, "center": {"x": 4, "y": 4, "w": 10, "h": 4}}]}]}
        })");
    };
    // clang-format on

    const animation2d::SpriteAtlas atlas = animation2d::SpriteAtlas::parse(trimmed(R"({"x": 4, "y": 3, "w": 18, "h": 12})"), texture);
    const graphics2d::NineSlice& slice = atlas.getSlice("panel");
    EXPECT_EQ(slice.pieces[0], (math::Rect{42.0F, 10.0F, 4.0F, 4.0F}));
    EXPECT_EQ(slice.pieces[8], (math::Rect{56.0F, 18.0F, 4.0F, 4.0F}));
    EXPECT_THROW((void)animation2d::SpriteAtlas::parse(trimmed(R"({"x": 0, "y": 0, "w": 24, "h": 18})"), texture), std::invalid_argument);
}

TEST_F(SpriteAtlasTest, ReadsTexturePackerHashesAndRejectsUnsupportedData) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(32, 32));

    const animation2d::SpriteAtlas hash = animation2d::SpriteAtlas::parse(nlohmann::ordered_json::parse(R"({"frames": {"a.png": {"frame": {"x": 0, "y": 0, "w": 8, "h": 8}}, "b.png": {"frame": {"x": 8, "y": 0, "w": 8, "h": 8}}}, "meta": {"image": "sheet.png"}})"), texture);
    EXPECT_EQ(hash.getFrameNames().size(), 2U);
    EXPECT_EQ(hash.getFrame("b.png").source.x, 8.0F);
    EXPECT_TRUE(hash.getAnimationNames().empty());

    const auto parse = [&texture](const char* text) { return animation2d::SpriteAtlas::parse(nlohmann::ordered_json::parse(text), texture); };
    EXPECT_THROW((void)parse(R"({"frames": {"a": {"frame": {"x": 0, "y": 0, "w": 8, "h": 8}, "rotated": true}}})"), std::invalid_argument);
    EXPECT_THROW((void)parse(R"({"frames": [{"filename": "a", "frame": {"x": 0, "y": 0, "w": 8, "h": 8}}], "meta": {"frameTags": [{"name": "t", "from": 0, "to": 3}]}})"), std::out_of_range);
    EXPECT_THROW((void)parse(R"({"frames": [{"filename": "a", "frame": {"x": 0, "y": 0, "w": 8, "h": 8}}], "meta": {"frameTags": [{"name": "t", "from": 0, "to": 0, "direction": "sideways"}]}})"), std::invalid_argument);
    EXPECT_THROW((void)parse(R"({"frames": [{"filename": "a", "frame": {"x": 0, "y": 0, "w": 8, "h": 8}}], "meta": {"frameTags": [{"name": "t", "from": 0, "to": 0, "repeat": "3"}]}})"), std::invalid_argument);
}

TEST_F(SpriteAtlasTest, KeepsHashFramesInFileOrder) {
    // Aseprite names frames `hero 0` to `hero 11` and its tags count positions in the file, where `hero 10` comes after `hero 9`.
    std::string frames;
    for (int index = 0; index < 12; ++index) {
        frames += (index == 0 ? "" : ", ") + std::string(R"("hero )") + std::to_string(index) + R"(": {"frame": {"x": )" + std::to_string(index * 8) + R"(, "y": 0, "w": 8, "h": 8}})";
    }
    const std::vector<std::uint8_t> image = test::TestFiles::pngImage(96, 8, 0xFFFFFFFFU);
    test::EngineFixture fixture({{"content/hero.json", R"({"frames": {)" + frames + R"(}, "meta": {"image": "hero.png", "frameTags": [{"name": "walk", "from": 0, "to": 11}]}})"}, {"content/hero.png", std::string(image.begin(), image.end())}});

    const auto atlas = std::static_pointer_cast<animation2d::SpriteAtlas>(fixture.engine().getAssets().load("atlas", "hero.json"));
    EXPECT_EQ(atlas->getFrameNames()[10], "hero 10");
    const animation2d::Animation& walk = atlas->getAnimation("walk");
    ASSERT_EQ(walk.frames.size(), 12U);
    for (std::size_t index = 0; index < walk.frames.size(); ++index) {
        EXPECT_EQ(walk.frames[index].source.x, static_cast<float>(index * 8)) << index;
    }
}

TEST_F(SpriteAtlasTest, LoadsThroughAssetsAndSharesItsTexture) {
    const std::vector<std::uint8_t> image = test::TestFiles::pngImage(64, 32, 0xFFFFFFFFU);
    test::EngineFixture fixture({{"content/ui/hero.json", kAsepriteAtlas}, {"content/ui/hero.png", std::string(image.begin(), image.end())}});
    assets::Manager& assets = fixture.engine().getAssets();

    const auto atlas = std::static_pointer_cast<animation2d::SpriteAtlas>(assets.load("atlas", "ui/hero.json"));
    EXPECT_EQ(atlas->getTexture(), assets.texture("ui/hero.png"));
    EXPECT_EQ(std::static_pointer_cast<animation2d::SpriteAtlas>(assets.load("atlas", "ui/hero.json", {{"filter", "nearest"}})), atlas);

    std::shared_ptr<animation2d::SpriteAtlas> smooth;
    // clang-format off
    assets.loadAsync("atlas", "ui/hero.json", [&](std::shared_ptr<void> asset, std::string) {
        smooth = std::static_pointer_cast<animation2d::SpriteAtlas>(asset);
    }, {{"filter", "linear"}});
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&] { return smooth != nullptr; }));
    EXPECT_NE(smooth, atlas);
    EXPECT_EQ(smooth->getTexture(), assets.texture("ui/hero.png", {.filter = graphics::Texture::Filter::Linear}));

    std::string error;
    assets.loadAsync("atlas", "ui/missing.json", [&](std::shared_ptr<void>, std::string message) { error = message; });
    ASSERT_TRUE(fixture.frameUntil([&] { return !error.empty(); }));
    EXPECT_THROW((void)assets.load("atlas", "ui/hero.json", {{"scale", 2}}), std::invalid_argument);
}

TEST_F(Animation2DLuaTest, AnimatesSpritesFromLua) {
    const std::vector<std::uint8_t> image = test::TestFiles::pngImage(64, 32, 0xFFFFFFFFU);
    test::EngineFixture fixture({{"content/ui/hero.json", kAsepriteAtlas}, {"content/ui/hero.png", std::string(image.begin(), image.end())}, {"content/units/warrior.png", std::string(image.begin(), image.end())}});
    // clang-format off
    fixture.runLua(R"(
        animation2d = require('haylen.animation2d')
        graphics2d = require('haylen.graphics2d')
        assets = require('haylen.assets')
        sheet = assets.texture('units/warrior.png')
        run = animation2d.fromGrid(sheet, {frameWidth = 16, frameHeight = 16, framesPerSecond = 10})
        slash = animation2d.fromGrid(sheet, {frameWidth = 16, frameHeight = 16, cells = {2, 3}, framesPerSecond = 5, loop = 'once', margin = {0, 0}, spacing = {0, 0}})
        animator = animation2d.newAnimator()
        animator:add('run', run)
        animator:add('slash', slash)
        events = {}
        animator.onFrame = function(name, frame) events[#events + 1] = name .. frame end
        animator.onFinish = function(name) events[#events + 1] = name .. ' done' animator:play('run') end
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("return run.frameCount .. ' ' .. string.format('%.2f', run.duration) .. ' ' .. run.loop .. ' ' .. tostring(run.texture == sheet)"), "8 0.80 loop true");
    EXPECT_EQ(fixture.lua("local r = slash:frame(1) return r.x .. ' ' .. slash:frameAt(1)"), "16.0 2");
    EXPECT_EQ(fixture.lua("animator:play('slash') animator:update(0.25) animator:update(0.3) return table.concat(events, ',')"), "slash2,slash done");
    EXPECT_EQ(fixture.lua("return animator.current .. ' ' .. animator.frame .. ' ' .. tostring(animator.playing) .. ' ' .. tostring(animator.finished) .. ' ' .. animator.time"), "run 1 true false 0.0");
    EXPECT_EQ(fixture.lua("local sprite = graphics2d.newSprite(sheet) animator.speed = 2 animator.pivotY = 1 animator:update(0.1) animator:apply(sprite) return sprite.source.x .. ' ' .. sprite.pivotY"), "32.0 1.0");
    EXPECT_EQ(fixture.lua("return type(animator.onFrame) .. ' ' .. tostring(animator:has('run'))"), "function true");
    EXPECT_EQ(fixture.lua("animator.onFrame = nil animator:stop() return tostring(animator.onFrame) .. ' ' .. tostring(animator.playing)"), "nil false");
    EXPECT_EQ(fixture.lua("return animation2d.newAnimator().current"), "nil");
    EXPECT_EQ(fixture.lua("animator.onFinish = nil animator.speed = 1 animator:play('slash') animator:queue('run') local before = animator.queuedCount animator:update(0.5) return before .. ' ' .. animator.queuedCount .. ' ' .. animator.current"), "1 0 run");
    EXPECT_EQ(fixture.lua("animator:queue('slash') animator:clearQueue() return animator.queuedCount"), "0");
    EXPECT_EQ(fixture.lua("animator:play('slash', true) animator:update(5) animator:queue('slash') return animator.frame .. ' ' .. tostring(animator.finished) .. ' ' .. tostring(animator.playing)"), "1 false true");
    EXPECT_EQ(fixture.lua("local a = animator:animation('slash') return a.frameCount .. ' ' .. a.loop .. ' ' .. string.format('%.2f %.2f', run.cycleDuration, animation2d.fromGrid(sheet, {frameWidth = 16, frameHeight = 16, loop = 'pingPong'}).cycleDuration)"), "2 once 0.80 1.40");
    EXPECT_NE(fixture.lua("animator:animation('fly')").find("The animation \"fly\" does not exist in this animator."), std::string::npos);

    fixture.runLua("atlas = assets.load('ui/hero.json', 'atlas')");
    EXPECT_EQ(fixture.lua("return #atlas:frameNames() .. ' ' .. table.concat(atlas:animationNames(), ',') .. ' ' .. table.concat(atlas:sliceNames(), ',')"), "4 back,bounce,hit,walk panel");
    EXPECT_EQ(fixture.lua("return atlas:animation('walk').frameCount .. ' ' .. atlas:source('hero 1').x .. ' ' .. tostring(atlas.texture == assets.texture('ui/hero.png'))"), "3 8.0 true");
    EXPECT_EQ(fixture.lua("return tostring(atlas == assets.load('ui/hero.json', 'atlas')) .. ' ' .. tostring(atlas == assets.load('ui/hero.json', 'atlas', {filter = 'linear'})) .. ' ' .. tostring(atlas == atlas.texture)"), "true false false");
    EXPECT_EQ(fixture.lua("local s = graphics2d.newSprite(atlas.texture) atlas:apply(s, 'hero 0') return s.width .. ' ' .. s.pivotX"), "8.0 0.5");
    EXPECT_EQ(fixture.lua("return atlas:animationFromFrames({'hero 1', 'hero 2'}, {framesPerSecond = 2, loop = 'pingPong'}).duration"), "1.0");
    EXPECT_EQ(fixture.lua("return type(atlas:slice('panel'))"), "userdata");
    EXPECT_EQ(fixture.lua("return tostring(atlas:hasFrame('hero 0')) .. tostring(atlas:hasFrame('nope')) .. tostring(atlas:hasAnimation('walk')) .. tostring(atlas:hasAnimation('fly')) .. tostring(atlas:hasSlice('panel')) .. tostring(atlas:hasSlice('marker'))"), "truefalsetruefalsetruefalse");
    EXPECT_EQ(fixture.lua("local f = atlas:frame('hero 0') return f.source.width .. ' ' .. f.offset.x .. ' ' .. f.originalSize.x .. ' ' .. string.format('%.2f %.2f', f.duration, atlas:frame('hero 1').duration)"), "8.0 4.0 16.0 0.10 0.20");
    EXPECT_EQ(fixture.lua("return animation2d.fromFrames(sheet, {{0, 0, 8, 8}, {8, 0, 8, 8}}, {framesPerSecond = 4}).duration"), "0.5");

    EXPECT_NE(fixture.lua("return animation2d.fromGrid(sheet, {frameWidth = 16, frameHeight = 16, speed = 2})").find("Unknown option \"speed\""), std::string::npos);
    EXPECT_NE(fixture.lua("return animation2d.fromGrid(sheet, {frameWidth = 16, frameHeight = 16, framesPerSecond = 0})").find("framesPerSecond must be positive"), std::string::npos);
    EXPECT_NE(fixture.lua("return animation2d.fromFrames(sheet, {})").find("expected at least one frame"), std::string::npos);
    EXPECT_NE(fixture.lua("return run:frame(9)").find("frame index out of range"), std::string::npos);
    EXPECT_NE(fixture.lua("animator:play('fly')").find("The animation \"fly\" does not exist in this animator."), std::string::npos);
    EXPECT_NE(fixture.lua("animator.onFinish = 3").find("error: "), std::string::npos);
    EXPECT_NE(fixture.lua("return atlas:source('nope')").find("The frame \"nope\" does not exist in this atlas."), std::string::npos);

    fixture.runLua("animator:play('slash', true) animator.onFinish = function() error('finish failed') end");
    EXPECT_NE(fixture.lua("animator:update(5)").find("finish failed"), std::string::npos);
}

} // namespace haylen::animation2d
