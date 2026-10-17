#include <gtest/gtest.h>

#include <cmath>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/2d/animation/Animation.hpp"
#include "haylen/2d/animation/Animator.hpp"
#include "haylen/2d/animation/SpriteAtlas.hpp"
#include "haylen/2d/graphics/Sprite.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "support/EngineFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::animation2d {

class FrameDataTest : public ::testing::Test {
  protected:
    // Four frames of 32 by 32, the second trimmed by 8 pixels on its left, with a hitbox from the second frame to the third, a hand point on every frame and a pivot on the feet.
    static constexpr const char* kAtlas = R"({
        "frames": [
            {"filename": "punch 0", "frame": {"x": 0, "y": 0, "w": 32, "h": 32}, "duration": 100},
            {"filename": "punch 1", "frame": {"x": 32, "y": 0, "w": 24, "h": 32}, "spriteSourceSize": {"x": 8, "y": 0, "w": 24, "h": 32}, "sourceSize": {"w": 32, "h": 32}, "duration": 100, "data": "swing, whoosh"},
            {"filename": "punch 2", "frame": {"x": 56, "y": 0, "w": 32, "h": 32}, "duration": 100},
            {"filename": "punch 3", "frame": {"x": 88, "y": 0, "w": 32, "h": 32}, "duration": 100, "data": "recover"}
        ],
        "meta": {
            "image": "fighter.png",
            "frameTags": [{"name": "punch", "from": 0, "to": 3, "direction": "forward"}],
            "slices": [
                {"name": "hit", "keys": [{"frame": 1, "bounds": {"x": 20, "y": 8, "w": 10, "h": 6}, "pivot": {"x": -4, "y": 22}}, {"frame": 3, "bounds": {"x": 0, "y": 0, "w": 0, "h": 0}}]},
                {"name": "hand", "data": "point", "keys": [{"frame": 0, "bounds": {"x": 18, "y": 12, "w": 4, "h": 4}}]}
            ]
        }
    })";

    [[nodiscard]] static SpriteAtlas parseAtlas(test::EngineFixture& fixture) {
        return SpriteAtlas::parse(nlohmann::ordered_json::parse(kAtlas), fixture.engine().getGraphics().createTexture(graphics::Image(128, 32)));
    }

    // An animation of four frames of a tenth of a second, or of one, two, three and four ticks.
    [[nodiscard]] static Animation makeStrip(const graphics::Texture& texture, Animation::Loop loop, bool ticks) {
        Animation animation{.texture = texture, .loop = loop};
        for (std::uint32_t index = 0; index < 4; ++index) {
            animation.frames.push_back({.source = {static_cast<float>(index) * 16.0F, 0.0F, 16.0F, 16.0F}, .originalSize = {16.0F, 16.0F}, .duration = 0.1F, .ticks = ticks ? index + 1 : 0, .events = {"enter" + std::to_string(index)}});
        }
        return animation;
    }
};

TEST_F(FrameDataTest, ReadsBoxesPointsPivotsAndEventsFromAtlases) {
    test::EngineFixture fixture;
    const SpriteAtlas atlas = parseAtlas(fixture);

    EXPECT_EQ(atlas.getFrame("punch 0").findBox("hit"), nullptr);
    ASSERT_NE(atlas.getFrame("punch 1").findBox("hit"), nullptr);
    EXPECT_EQ(atlas.getFrame("punch 1").findBox("hit")->area, (math::Rect{20.0F, 8.0F, 10.0F, 6.0F}));
    EXPECT_NE(atlas.getFrame("punch 2").findBox("hit"), nullptr);
    EXPECT_EQ(atlas.getFrame("punch 3").findBox("hit"), nullptr);
    EXPECT_EQ(atlas.getFrame("punch 2").pivot, math::Vec2(16.0F, 30.0F));
    EXPECT_FALSE(atlas.getFrame("punch 0").pivot.has_value());
    EXPECT_EQ(atlas.getFrame("punch 3").findPoint("hand")->position, math::Vec2(20.0F, 14.0F));
    EXPECT_EQ(atlas.getFrame("punch 1").events, (std::vector<std::string>{"swing", "whoosh"}));
    EXPECT_FALSE(atlas.hasSlice("hit"));

    // Tags copy the frames with their data.
    EXPECT_EQ(atlas.getAnimation("punch").frames[3].events, (std::vector<std::string>{"recover"}));
    EXPECT_NE(atlas.getAnimation("punch").frames[1].findBox("hit"), nullptr);
}

// A frame that reaches past the image of the atlas would draw pixels the image does not hold, so the atlas refuses it.
TEST_F(FrameDataTest, RefusesFramesOutsideTheImageOfTheAtlas) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(64, 32));
    // clang-format off
    const auto parse = [&](const char* frame) {
        return SpriteAtlas::parse(nlohmann::ordered_json::parse(std::string(R"({"frames": {"grip": {"frame": )") + frame + "}}}"), texture);
    };
    // clang-format on
    EXPECT_NO_THROW((void)parse(R"({"x": 32, "y": 0, "w": 32, "h": 32})"));
    for (const char* frame : {R"({"x": 32, "y": 2, "w": 32, "h": 32})", R"({"x": 40, "y": 0, "w": 32, "h": 32})", R"({"x": -1, "y": 0, "w": 8, "h": 8})", R"({"x": 0, "y": 0, "w": 0, "h": 8})"}) {
        SCOPED_TRACE(frame);
        try {
            (void)parse(frame);
            ADD_FAILURE() << "The atlas took a frame outside its image.";
        } catch (const std::invalid_argument& error) {
            EXPECT_STREQ(error.what(), "The frame \"grip\" of the atlas must lie inside its image of 64 by 32 pixels, with an area.");
        }
    }
}

TEST_F(FrameDataTest, RunsTheEventsOfEveryFrameItPassesInOrder) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(64, 16));
    Animator animator;
    animator.add("loop", makeStrip(texture, Animation::Loop::Loop, false));
    animator.add("bounce", makeStrip(texture, Animation::Loop::PingPong, false));
    std::vector<std::string> heard;
    animator.onEvent = [&heard](std::string_view animation, std::string_view event, std::size_t frame) { heard.push_back(std::string(animation) + ":" + std::string(event) + ":" + std::to_string(frame)); };

    animator.play("loop");
    animator.update(0.25F);
    EXPECT_EQ(heard, (std::vector<std::string>{"loop:enter0:0", "loop:enter1:1", "loop:enter2:2"}));

    // A long update wraps around and enters the first frame again.
    heard.clear();
    animator.update(0.2F);
    EXPECT_EQ(heard, (std::vector<std::string>{"loop:enter3:3", "loop:enter0:0"}));

    heard.clear();
    animator.play("bounce");
    animator.update(0.65F);
    EXPECT_EQ(heard, (std::vector<std::string>{"bounce:enter0:0", "bounce:enter1:1", "bounce:enter2:2", "bounce:enter3:3", "bounce:enter2:2", "bounce:enter1:1", "bounce:enter0:0"}));

    // A callback that switches animations stops the events of the old one.
    heard.clear();
    // clang-format off
    animator.onEvent = [&](std::string_view animation, std::string_view event, std::size_t) {
        heard.push_back(std::string(animation) + ":" + std::string(event));
        if (event == "enter1") {
            animator.play("bounce", true);
        }
    };
    // clang-format on
    animator.play("loop", true);
    animator.update(0.35F);
    EXPECT_EQ(heard, (std::vector<std::string>{"loop:enter0", "loop:enter1"}));
    EXPECT_EQ(animator.getCurrent(), "bounce");
}

TEST_F(FrameDataTest, StepsTickAnimationsExactly) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(64, 16));
    Animator animator;
    animator.add("combo", makeStrip(texture, Animation::Loop::Loop, true));
    animator.add("idle", makeStrip(texture, Animation::Loop::Loop, false));
    animator.play("combo");

    // The frames last 1, 2, 3 and 4 ticks, so a pass takes 10 steps and every frame starts on its own step.
    std::vector<std::size_t> starts;
    std::size_t previous = 99;
    for (int step = 0; step < 10000; ++step) {
        if (animator.getFrame() != previous) {
            previous = animator.getFrame();
            if (step < 10) {
                starts.push_back(static_cast<std::size_t>(step));
            }
        }
        animator.step(1);
    }
    EXPECT_EQ(starts, (std::vector<std::size_t>{0, 1, 3, 6}));
    EXPECT_EQ(animator.getFrame(), 0U);
    EXPECT_FLOAT_EQ(animator.getTime(), 10000.0F);

    try {
        animator.update(0.1F);
        FAIL();
    } catch (const std::logic_error& error) {
        EXPECT_STREQ(error.what(), "The animation \"combo\" counts ticks, so it advances with \"step\".");
    }
    animator.play("idle");
    EXPECT_THROW(animator.step(1), std::logic_error);

    Animation mixed = makeStrip(texture, Animation::Loop::Loop, true);
    mixed.frames[2].ticks = 0;
    EXPECT_THROW(animator.add("mixed", mixed), std::invalid_argument);
}

TEST_F(FrameDataTest, PlacesBoxesAndPointsWithTheSprite) {
    test::EngineFixture fixture;
    const SpriteAtlas atlas = parseAtlas(fixture);
    Animator animator;
    animator.add("punch", atlas.getAnimation("punch"));
    animator.play("punch");
    animator.update(0.15F);
    ASSERT_EQ(animator.getFrame(), 1U);

    // The trimmed frame keeps its pivot on the feet, and a flip mirrors it inside the quad.
    graphics2d::Sprite sprite{.position = {100.0F, 200.0F}};
    animator.apply(sprite);
    EXPECT_EQ(sprite.pivot, math::Vec2(8.0F / 24.0F, 30.0F / 32.0F));
    const math::Rect box = *animator.getBox("hit", sprite);
    EXPECT_FLOAT_EQ(box.x, 104.0F);
    EXPECT_FLOAT_EQ(box.y, 178.0F);
    EXPECT_FLOAT_EQ(box.width, 10.0F);

    sprite.flip.horizontal = true;
    animator.apply(sprite);
    const math::Rect mirrored = *animator.getBox("hit", sprite);
    EXPECT_FLOAT_EQ(mirrored.x, 86.0F);
    EXPECT_FLOAT_EQ(mirrored.getRight(), 96.0F);

    // A quarter turn and a double scale turn the hand around the feet.
    sprite.flip.horizontal = false;
    animator.apply(sprite);
    sprite.rotation = std::numbers::pi_v<float> * 0.5F;
    sprite.scale = {2.0F, 2.0F};
    const math::Vec2 hand = *animator.getPoint("hand", sprite);
    EXPECT_NEAR(hand.x, 100.0F + 32.0F, 1.0e-3F);
    EXPECT_NEAR(hand.y, 200.0F + 8.0F, 1.0e-3F);
    EXPECT_FALSE(animator.getBox("guard", sprite).has_value());
}

TEST_F(FrameDataTest, ExposesFrameDataToLua) {
    const std::vector<std::uint8_t> image = test::TestFiles::pngImage(128, 32, 0xFFFFFFFFU);
    test::EngineFixture fixture({{"content/fighter.json", kAtlas}, {"content/fighter.png", std::string(image.begin(), image.end())}});
    // clang-format off
    fixture.runLua(R"(
        animation2d = require('haylen.animation2d')
        assets = require('haylen.assets')
        graphics2d = require('haylen.graphics2d')
        sheet = assets.texture('fighter.png')
        punch = animation2d.fromFrames(sheet, {
            {source = {0, 0, 32, 32}, pivot = {16, 30}, ticks = 2},
            {source = {32, 0, 32, 32}, pivot = {14, 30}, ticks = 3, events = {'swing'}, boxes = {{name = 'hit', x = 20, y = 8, width = 10, height = 6}}, points = {{name = 'hand', x = 24, y = 12}}},
            {source = {64, 0, 32, 32}, ticks = 1},
        }, {loop = 'once'})
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return punch:frameData(2).boxes[1].width .. ' ' .. punch:frameData(2).events[1] .. ' ' .. punch:frameData(1).pivot.x .. ' ' .. tostring(punch.countsTicks)"), "10.0 swing 16.0 true");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(animation2d.fromFrames, sheet, {{source = {0, 0, 8, 8}, ticks = 2}, {0, 0, 8, 8}}) return message:match('Every.*')"), "Every frame of an animation counts ticks, or none does.");
    // clang-format off
    EXPECT_EQ(fixture.lua(R"(
        local heard = {}
        local animator = animation2d.newAnimator()
        animator:add('punch', punch)
        animator.onEvent = function(name, event, frame) heard[#heard + 1] = name .. ':' .. event .. ':' .. frame end
        animator:play('punch')
        animator:step(2)
        local sprite = graphics2d.newSprite(sheet, {x = 100, y = 200})
        animator:apply(sprite)
        local box = animator:box('hit', sprite)
        local x, y = animator:point('hand', sprite)
        return table.concat(heard, ',') .. ' ' .. box.x .. ' ' .. x .. ' ' .. y .. ' ' .. tostring(animator:box('guard', sprite))
    )"), "punch:swing:2 106.0 110.0 182.0 nil");
    // clang-format on
    EXPECT_EQ(fixture.lua("local animator = animation2d.newAnimator() animator:add('punch', punch) animator:play('punch') local ok, message = pcall(animator.update, animator, 0.1) return message:match('The animation.*')"), "The animation \"punch\" counts ticks, so it advances with \"step\".");
    EXPECT_EQ(fixture.lua("local anim = animation2d.fromGrid(sheet, {frameWidth = 32, frameHeight = 32, ticksPerFrame = 4}) anim:addEvent(2, 'step') return anim:frameData(2).events[1] .. ' ' .. anim:frameData(1).ticks"), "step 4");
    EXPECT_EQ(fixture.lua("local atlas = assets.load('fighter.json', 'atlas') local frame = atlas:frame('punch 1') return frame.boxes[1].name .. ' ' .. frame.events[2] .. ' ' .. frame.pivot.y"), "hit whoosh 30.0");
}

} // namespace haylen::animation2d
