#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/2d/animation/AnimatedBatch.hpp"
#include "haylen/2d/animation/Animation.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "support/EngineFixture.hpp"
#include "support/FrameRenderer.hpp"
#include "support/TestFiles.hpp"

namespace haylen::animation2d {

class AnimatedBatchTest : public ::testing::Test {
  protected:
    // Four frames of 16 by 16 in a row, the third trimmed to its right half with a pivot on its bottom middle.
    [[nodiscard]] static Animation makeStrip(const graphics::Texture& texture, Animation::Loop loop) {
        Animation animation{.texture = texture, .loop = loop};
        for (int index = 0; index < 4; ++index) {
            animation.frames.push_back({.source = {static_cast<float>(index) * 16.0F, 0.0F, 16.0F, 16.0F}, .originalSize = {16.0F, 16.0F}, .duration = 0.1F});
        }
        animation.frames[2].source = {40.0F, 0.0F, 8.0F, 16.0F};
        animation.frames[2].offset = {8.0F, 0.0F};
        animation.frames[2].pivot = math::Vec2{8.0F, 16.0F};
        return animation;
    }
};

class AnimatedBatchLuaTest : public AnimatedBatchTest {};

TEST_F(AnimatedBatchTest, AdvancesEverySpriteAtItsOwnSpeed) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(64, 16));
    AnimatedBatch batch(texture);
    batch.addAnimation("walk", makeStrip(texture, Animation::Loop::Loop));
    batch.addAnimation("attack", makeStrip(texture, Animation::Loop::Once));

    const std::size_t slow = batch.add({.position = {10.0F, 20.0F}}, "walk", 0.5F);
    const std::size_t fast = batch.add({.flip = {.horizontal = true}}, "walk", 2.0F);
    const std::size_t striking = batch.add({}, "attack");
    batch.setScale(fast, {2.0F, 3.0F});
    batch.update(0.11F);

    EXPECT_EQ(batch.getFrame(slow), 0U);
    EXPECT_EQ(batch.getFrame(fast), 2U);
    EXPECT_EQ(batch.getFrame(striking), 1U);
    EXPECT_EQ(batch.get(slow).position, math::Vec2(10.0F, 20.0F));

    // The trimmed frame shows its own source, scaled, with its pivot mirrored by the flip.
    EXPECT_EQ(batch.get(fast).source, (math::Rect{40.0F, 0.0F, 8.0F, 16.0F}));
    EXPECT_EQ(batch.get(fast).size, math::Vec2(16.0F, 48.0F));
    EXPECT_EQ(batch.get(fast).pivot, math::Vec2(1.0F, 1.0F));

    // A one-shot animation ends once, in the update that reaches its end.
    std::vector<std::size_t> heard;
    batch.completed.connect([&heard](std::span<const std::size_t> ended) { heard.assign(ended.begin(), ended.end()); });
    batch.update(0.4F);
    EXPECT_EQ(heard, (std::vector<std::size_t>{striking}));
    EXPECT_EQ(std::vector<std::size_t>(batch.getFinished().begin(), batch.getFinished().end()), heard);
    heard.clear();
    batch.update(0.4F);
    EXPECT_TRUE(heard.empty());
    EXPECT_EQ(batch.getFrame(striking), 3U);

    batch.play(striking, "attack", true);
    EXPECT_EQ(batch.getFrame(striking), 0U);
    batch.remove(slow);
    EXPECT_EQ(batch.size(), 2U);
    EXPECT_EQ(batch.getAnimation(1), "attack");
}

TEST_F(AnimatedBatchTest, ReplacesAnimationsAndHearsEndsWhileListenersUpdate) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(64, 16));
    AnimatedBatch batch(texture);
    batch.addAnimation("walk", makeStrip(texture, Animation::Loop::Loop));
    const std::size_t walker = batch.add({}, "walk");
    batch.update(0.35F);
    ASSERT_EQ(batch.getFrame(walker), 3U);

    // A shorter animation in place of the one a sprite plays shows the frame of its time among the new frames.
    Animation shorter{.texture = texture, .loop = Animation::Loop::Loop};
    shorter.frames.push_back({.source = {0.0F, 0.0F, 16.0F, 16.0F}, .originalSize = {16.0F, 16.0F}, .duration = 0.2F});
    shorter.frames.push_back({.source = {16.0F, 0.0F, 16.0F, 16.0F}, .originalSize = {16.0F, 16.0F}, .duration = 0.2F});
    batch.addAnimation("walk", shorter);
    EXPECT_EQ(batch.getFrame(walker), 1U);
    EXPECT_EQ(batch.get(walker).source, (math::Rect{16.0F, 0.0F, 16.0F, 16.0F}));

    // A listener that updates the batch again leaves the list the other listeners hear whole.
    batch.addAnimation("attack", makeStrip(texture, Animation::Loop::Once));
    const std::size_t first = batch.add({}, "attack");
    const std::size_t second = batch.add({}, "attack");
    std::vector<std::size_t> heard;
    batch.completed.connect([&batch](std::span<const std::size_t>) { batch.update(0.0F); });
    batch.completed.connect([&heard](std::span<const std::size_t> ended) { heard.assign(ended.begin(), ended.end()); });
    batch.update(0.5F);
    EXPECT_EQ(heard, (std::vector<std::size_t>{first, second}));
}

TEST_F(AnimatedBatchTest, UpdatesLargeBatchesOnTheJobSystemLikeOnOneThread) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(64, 16));
    AnimatedBatch serial(texture);
    AnimatedBatch parallel(texture);
    for (AnimatedBatch* batch : {&serial, &parallel}) {
        batch->addAnimation("walk", makeStrip(texture, Animation::Loop::PingPong));
        for (int index = 0; index < 10000; ++index) {
            (void)batch->add({}, "walk", 0.5F + static_cast<float>(index % 7) * 0.25F);
        }
    }
    for (int frame = 0; frame < 30; ++frame) {
        serial.update(1.0F / 60.0F);
        parallel.update(1.0F / 60.0F, &fixture.engine().getJobs());
    }
    for (std::size_t index = 0; index < serial.size(); index += 97) {
        EXPECT_EQ(serial.getFrame(index), parallel.getFrame(index));
        EXPECT_EQ(serial.get(index).source, parallel.get(index).source);
    }
}

TEST_F(AnimatedBatchTest, DrawsEverySpriteInOneCall) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(64, 16));
    AnimatedBatch batch(texture);
    batch.addAnimation("walk", makeStrip(texture, Animation::Loop::Loop));
    for (int index = 0; index < 500; ++index) {
        (void)batch.add({.position = {static_cast<float>(index), 0.0F}}, "walk", static_cast<float>(index % 3));
    }
    std::vector<float> places{1.0F, 2.0F, 3.0F, 4.0F};
    batch.writeFields(places, graphics2d::SpriteLayout({graphics2d::SpriteLayout::Field::X, graphics2d::SpriteLayout::Field::Y}), 10);
    EXPECT_EQ(batch.get(11).position, math::Vec2(3.0F, 4.0F));
    batch.update(0.25F);

    // clang-format off
    const graphics2d::Renderer::Stats stats = test::FrameRenderer::renderOnce(fixture, [&batch](core::Engine& engine) {
        engine.getRenderer2D().beginScreen();
        batch.draw(engine.getRenderer2D());
    });
    // clang-format on
    EXPECT_EQ(stats.sprites, 500U);
    EXPECT_EQ(stats.drawCalls, 1U);
}

TEST_F(AnimatedBatchTest, RefusesAnimationsItCannotPlay) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(64, 16));
    const graphics::Texture other = fixture.engine().getGraphics().createTexture(graphics::Image(64, 16));
    AnimatedBatch batch(texture);
    batch.addAnimation("walk", makeStrip(texture, Animation::Loop::Loop));
    EXPECT_THROW(batch.addAnimation("other", makeStrip(other, Animation::Loop::Loop)), std::invalid_argument);
    Animation ticked = makeStrip(texture, Animation::Loop::Loop);
    for (SpriteFrame& frame : ticked.frames) {
        frame.ticks = 2;
    }
    EXPECT_THROW(batch.addAnimation("ticked", ticked), std::invalid_argument);
    try {
        (void)batch.add({}, "run");
        FAIL();
    } catch (const std::invalid_argument& error) {
        EXPECT_STREQ(error.what(), "The animation \"run\" does not exist in this animated batch.");
    }
    EXPECT_THROW(batch.step(1), std::logic_error);
    EXPECT_THROW(batch.play(4, "walk"), std::out_of_range);

    AnimatedBatch steps(texture);
    steps.addAnimation("ticked", ticked);
    (void)steps.add({}, "ticked");
    steps.step(5);
    EXPECT_EQ(steps.getFrame(0), 2U);
    EXPECT_THROW(steps.update(0.1F), std::logic_error);
}

TEST_F(AnimatedBatchLuaTest, PlaysCrowdsFromLua) {
    const std::vector<std::uint8_t> image = test::TestFiles::pngImage(64, 16, 0xFFFFFFFFU);
    test::EngineFixture fixture({{"content/units.png", std::string(image.begin(), image.end())}});
    // clang-format off
    EXPECT_EQ(fixture.lua(R"(
        animation2d = require('haylen.animation2d')
        assets = require('haylen.assets')
        local sheet = assets.texture('units.png')
        local walk = animation2d.fromGrid(sheet, {frameWidth = 16, frameHeight = 16, framesPerSecond = 10})
        local die = animation2d.fromGrid(sheet, {frameWidth = 16, frameHeight = 16, cells = {3, 4}, framesPerSecond = 10, loop = 'once'})
        crowd = animation2d.newAnimatedBatch(sheet, {walk = walk, die = die})
        for index = 1, 20 do
            crowd:add({x = index * 20, y = 10, animation = index % 5 == 0 and 'die' or 'walk', speed = 1 + index % 2})
        end
        ended = {}
        crowd:on('complete', function(list) for _, index in ipairs(list) do ended[#ended + 1] = index end end, {owner = crowd})
        crowd:update(0.25)
        local sprite = crowd:get(2)
        return crowd:size() .. ' ' .. table.concat(ended, ',') .. ' ' .. sprite.animation .. ' ' .. sprite.frame .. ' ' .. sprite.source.x
    )"), "20 5,10,15,20 walk 3 32.0");
    // clang-format on
    EXPECT_EQ(fixture.lua("crowd:play(1, 'die', true) crowd:set(1, {speed = 2, scaleX = 2}) crowd:update(0.1) return crowd:animation(1) .. ' ' .. crowd:frame(1) .. ' ' .. crowd:get(1).width .. ' ' .. #crowd:finished()"), "die 2 32.0 1");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(crowd.add, crowd, {animation = 'fly'}) return message:match('The animation.*')"), "The animation \"fly\" does not exist in this animated batch.");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(crowd.on, crowd, 'loop', function() end) return message:match('Unknown.*')"), "Unknown animated batch event \"loop\". Animated batches report \"complete\".");
}

} // namespace haylen::animation2d
