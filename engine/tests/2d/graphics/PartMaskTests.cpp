#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include "2d/graphics/GpuInstance.hpp"
#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/PartColors.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/SpriteBatch.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "support/EngineFixture.hpp"
#include "support/FrameRenderer.hpp"

namespace haylen::graphics2d {

namespace {

class PartMaskTest : public ::testing::Test {
  protected:
    [[nodiscard]] graphics::Texture makeTexture() {
        return fixture.engine().getGraphics().createTexture(graphics::Image(8, 8, math::Color::white()));
    }

    std::string lua(const std::string& source) {
        return fixture.lua(source);
    }

    test::EngineFixture fixture;
};

} // namespace

// The record after a recolored sprite holds the colors of its red, green, blue and yellow parts in its first sixteen bytes.
TEST_F(PartMaskTest, PacksThePartColorsIntoTheRecordAfterTheSprite) {
    const PartColors colors{.red = math::Color::fromHex(0xC03030FFU), .green = math::Color::fromHex(0x30C030FFU), .blue = math::Color::fromHex(0x3030C080U), .yellow = math::Color::transparent()};
    const GpuInstance packed = GpuInstance::makeParts(colors);
    std::uint32_t read[4]{};
    std::memcpy(read, &packed, sizeof(read));
    EXPECT_EQ(read[0], colors.red.toRgba8());
    EXPECT_EQ(read[1], colors.green.toRgba8());
    EXPECT_EQ(read[2], colors.blue.toRgba8());
    EXPECT_EQ(read[3], 0U);
}

// Recolored sprites take two records each and still share one draw call when they share their texture and mask, while sprites without a mask draw apart.
TEST_F(PartMaskTest, DrawsRecoloredSpritesAndBatchesInOneCall) {
    const graphics::Texture texture = makeTexture();
    const graphics::Texture mask = makeTexture();
    const std::vector<SpriteInstance> sprites(5, SpriteInstance{.size = {8.0F, 8.0F}});
    const std::vector<PartColors> colors{PartColors{.red = math::Color::black()}};

    // clang-format off
    const Renderer::Stats stats = test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        Renderer& renderer = engine.getRenderer2D();
        renderer.beginScreen();
        renderer.draw({.texture = texture, .partColors = {.green = math::Color::black()}, .order = {.partMask = mask}});
        renderer.drawBatch(texture, sprites, {.partMask = mask}, colors);
        renderer.draw({.texture = texture});
    });
    // clang-format on

    EXPECT_EQ(stats.sprites, 7U);
    EXPECT_EQ(stats.instances, 13U);
    EXPECT_EQ(stats.drawCalls, 2U);

    // A canvas that sorts by y sorts every recolored sprite of a batch on its own.
    // clang-format off
    const Renderer::Stats sorted = test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        Camera camera;
        engine.getRenderer2D().beginWorld(camera, {.sort = Renderer::SortMode::Y});
        engine.getRenderer2D().drawBatch(texture, sprites, {.partMask = mask});
    });
    // clang-format on
    EXPECT_EQ(sorted.sprites, 5U);
    EXPECT_EQ(sorted.instances, 10U);
    EXPECT_EQ(sorted.drawCalls, 1U);
}

TEST_F(PartMaskTest, RejectsMaterialsBakedBatchesAndTheirOwnTarget) {
    const graphics::Texture texture = makeTexture();
    const graphics::RenderTarget target = fixture.engine().getGraphics().createRenderTarget(16, 16);
    Renderer& renderer = fixture.engine().getRenderer2D();
    const std::vector<SpriteInstance> sprites(2);
    const StaticSpriteBatch baked = renderer.createStaticBatch(texture, sprites);

    renderer.beginScreen();
    EXPECT_THROW(renderer.drawStatic(baked, {.partMask = texture}), std::invalid_argument);
    Camera camera;
    renderer.beginTarget(target, camera);
    EXPECT_THROW(renderer.draw({.texture = texture, .order = {.partMask = target.getTexture()}}), std::invalid_argument);
    fixture.frames(1);
}

TEST_F(PartMaskTest, KeepsThePartColorsOfABatchWithItsSprites) {
    SpriteBatch batch(makeTexture());
    batch.add({});
    batch.add({});
    EXPECT_EQ(batch.getPartColors(1), PartColors{});

    batch.setPartColors(1, {.blue = math::Color::black()});
    batch.add({});
    EXPECT_EQ(batch.getPartColors(1).blue, math::Color::black());
    EXPECT_EQ(batch.getPartColors(2), PartColors{});

    batch.remove(0);
    EXPECT_EQ(batch.getPartColors(0).blue, math::Color::black());
    batch.resize(4);
    EXPECT_EQ(batch.getPartColors(3), PartColors{});
    EXPECT_THROW(batch.setPartColors(4, {}), std::out_of_range);
    EXPECT_THROW((void)batch.getPartColors(4), std::out_of_range);
    batch.clear();
    batch.add({});
    EXPECT_EQ(batch.getPartColors(0), PartColors{});
}

TEST_F(PartMaskTest, RecolorsFromLua) {
    lua("graphics = require('haylen.graphics') graphics2d = require('haylen.graphics2d') base = graphics.newTexture(4, 4) mask = graphics.newTexture(4, 4)");
    lua("hero = graphics2d.newSprite(base, {partMask = mask, partColors = {red = '#FFC03030', yellow = '#FFE0C040'}})");
    EXPECT_EQ(lua("return tostring(hero.partMask == mask) .. ' ' .. hero.partColors.red:toHex() .. ' ' .. hero.partColors.green:toHex() .. ' ' .. hero.partColors.yellow:toHex()"), "true #FFC03030 #FFFFFFFF #FFE0C040");
    EXPECT_NE(lua("hero.partColors = {purple = '#FFFFFFFF'}").find("Unknown option \"purple\""), std::string::npos);

    lua("crowd = graphics2d.newSpriteBatch(base) crowd:add({x = 1}) crowd:add({x = 2, partColors = {blue = '#FF3030C0'}}) crowd:set(1, {partColors = {green = '#FF30C030'}})");
    EXPECT_EQ(lua("return crowd:get(1).partColors.green:toHex() .. ' ' .. crowd:get(2).partColors.blue:toHex() .. ' ' .. crowd:get(2).x"), "#FF30C030 #FF3030C0 2.0");

    // clang-format off
    lua(R"(
        scene = require('haylen.scene')
        scene.push({render = function()
            graphics2d.beginScreen()
            hero:draw()
            graphics2d.draw(base, 10, 10, {partMask = mask, partColors = {red = '#FF000000'}})
            crowd:draw({partMask = mask})
            graphics2d.drawBatch(base, {{x = 1, partColors = {red = '#FF102030'}}, {x = 2}}, {partMask = mask})
        end})
    )");
    // clang-format on
    fixture.frames(2);
    EXPECT_EQ(lua("local stats = graphics2d.stats() return stats.sprites .. ' ' .. stats.instances"), "6 12");
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

} // namespace haylen::graphics2d
