#include <gtest/gtest.h>

#include <cstring>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "2d/graphics/GpuInstance.hpp"
#include "graphics/TextureResource.hpp"
#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/SpriteBatch.hpp"
#include "haylen/2d/graphics/SpriteData.hpp"
#include "haylen/2d/graphics/SpriteLayout.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "support/AllocationTracker.hpp"
#include "support/DrawingScene.hpp"
#include "support/EngineFixture.hpp"
#include "support/FrameRenderer.hpp"

namespace haylen::graphics2d {

namespace {

class SpriteDataTest : public ::testing::Test {
  protected:
    [[nodiscard]] graphics::Texture makeTexture(int size = 8, graphics::Texture::Options options = {}) {
        return fixture.engine().getGraphics().createTexture(graphics::Image(size, size, math::Color::white()), options);
    }

    std::string lua(const std::string& source) {
        return fixture.lua(source);
    }

    test::EngineFixture fixture;
};

} // namespace

// The record after a sprite of the batch program holds its four numbers and then the index of its texture.
TEST_F(SpriteDataTest, PacksTheDataIntoTheRecordAfterTheSprite) {
    const GpuInstance packed = GpuInstance::makeData({.texture = 3, .values = {0.25F, -1.0F, 8.0F, 0.5F}});
    float read[5]{};
    std::memcpy(read, &packed, sizeof(read));
    EXPECT_EQ(read[0], 0.25F);
    EXPECT_EQ(read[1], -1.0F);
    EXPECT_EQ(read[2], 8.0F);
    EXPECT_EQ(read[3], 0.5F);
    EXPECT_EQ(read[4], 3.0F);
    EXPECT_EQ(packed.getDataTexture(), 3U);
}

// Sprites of three textures share one draw call with two records each, sorted by y or not, while data without a material or several textures keeps one record per sprite.
TEST_F(SpriteDataTest, DrawsTheSpritesOfSeveralTexturesInOneCall) {
    const std::vector<graphics::Texture> textures{makeTexture(8), makeTexture(16), makeTexture(32)};
    const std::vector<SpriteInstance> sprites(6, SpriteInstance{.size = {8.0F, 8.0F}});
    const std::vector<SpriteData> data{{.texture = 0}, {.texture = 1}, {.texture = 2}, {.texture = 1}, {.texture = 0}, {.texture = 2}};

    // clang-format off
    const Renderer::Stats stats = test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        engine.getRenderer2D().beginScreen();
        engine.getRenderer2D().drawBatch(textures, sprites, data);
    });
    // clang-format on
    EXPECT_EQ(stats.sprites, 6U);
    EXPECT_EQ(stats.instances, 12U);
    EXPECT_EQ(stats.drawCalls, 1U);

    // clang-format off
    const Renderer::Stats sorted = test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        engine.getRenderer2D().beginWorld(Camera{}, {.sort = Renderer::SortMode::Y});
        engine.getRenderer2D().drawBatch(textures, sprites, data);
    });
    // clang-format on
    EXPECT_EQ(sorted.instances, 12U);
    EXPECT_EQ(sorted.drawCalls, 1U);

    // clang-format off
    const Renderer::Stats plain = test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        engine.getRenderer2D().beginScreen();
        engine.getRenderer2D().drawBatch(std::span(textures).first(1), sprites, std::vector<SpriteData>(6, SpriteData{.values = {1.0F}}));
    });
    // clang-format on
    EXPECT_EQ(plain.instances, 6U);
    EXPECT_EQ(plain.drawCalls, 1U);

    // A drawn sprite of the batch names the texture it draws from.
    std::vector<std::string> labels;
    textures[1].getResource()->label = "middle.png";
    Renderer& renderer = fixture.engine().getRenderer2D();
    renderer.beginScreen();
    renderer.drawBatch(textures, std::span(sprites).first(1), std::span(data).subspan(1, 1));
    renderer.visitDrawn([&labels](const Renderer::Drawn& drawn) { labels.emplace_back(drawn.label); });
    fixture.frames(1);
    EXPECT_EQ(labels, std::vector<std::string>{"middle.png"});
}

TEST_F(SpriteDataTest, RejectsTexturesThatCannotShareABatch) {
    const graphics::Texture nearest = makeTexture();
    const graphics::Texture linear = makeTexture(8, {.filter = graphics::Texture::Filter::Linear});
    const std::vector<SpriteInstance> sprites(2);
    Renderer& renderer = fixture.engine().getRenderer2D();
    renderer.beginScreen();

    EXPECT_THROW(renderer.drawBatch(std::vector<graphics::Texture>{nearest, linear}, sprites, {}), std::invalid_argument);
    EXPECT_THROW(renderer.drawBatch(std::vector<graphics::Texture>(9, nearest), sprites, {}), std::invalid_argument);
    EXPECT_THROW(renderer.drawBatch(std::vector<graphics::Texture>{}, sprites, {}), std::invalid_argument);
    const std::vector<graphics::Texture> pair{nearest, makeTexture()};
    EXPECT_THROW(renderer.drawBatch(pair, sprites, std::vector<SpriteData>{{.texture = 2}}), std::invalid_argument);
    EXPECT_THROW(renderer.drawBatch(pair, sprites, {}, {.partMask = nearest}), std::invalid_argument);
    renderer.beginWorld(Camera{}, {.ambientLight = math::Color::white()});
    EXPECT_THROW(renderer.drawBatch(pair, sprites, {}, {.normalMap = nearest}), std::invalid_argument);
    EXPECT_THROW(renderer.drawBatch(pair, sprites, {}, {.distortion = 1.0F}), std::invalid_argument);
    fixture.frames(1);
}

TEST_F(SpriteDataTest, KeepsTheDataOfABatchWithItsSprites) {
    SpriteBatch batch(std::vector<graphics::Texture>{makeTexture(), makeTexture()});
    EXPECT_EQ(batch.getTextures().size(), 2U);
    batch.add({});
    batch.add({});
    EXPECT_EQ(batch.getData(1), SpriteData{});

    batch.setData(1, {.texture = 1, .values = {2.0F}});
    batch.add({});
    EXPECT_EQ(batch.getData(1).texture, 1U);
    EXPECT_EQ(batch.getData(2), SpriteData{});
    EXPECT_THROW(batch.setData(0, {.texture = 2}), std::invalid_argument);
    EXPECT_THROW(batch.setData(3, {}), std::out_of_range);

    batch.remove(0);
    EXPECT_EQ(batch.getData(0).values[0], 2.0F);
    batch.resize(4);
    EXPECT_EQ(batch.getData(3), SpriteData{});
    EXPECT_THROW((void)batch.bake(fixture.engine().getRenderer2D()), std::logic_error);

    // Fields of the data write every sprite, or none when a value names a texture the batch lacks, and read back with the texture counted from 1.
    const SpriteLayout layout({SpriteLayout::Field::Texture, SpriteLayout::Field::Data2});
    batch.writeFields(std::vector<float>{2.0F, 0.5F, 1.0F, 0.75F}, layout);
    EXPECT_EQ(batch.getData(0), (SpriteData{.texture = 1, .values = {2.0F, 0.5F}}));
    EXPECT_EQ(batch.getData(1), (SpriteData{.texture = 0, .values = {0.0F, 0.75F}}));
    EXPECT_THROW(batch.writeFields(std::vector<float>{1.0F, 0.0F, 3.0F, 0.0F}, layout), std::invalid_argument);
    EXPECT_EQ(batch.getData(0).texture, 1U);
    std::vector<float> read(4);
    batch.readFields(read, layout);
    EXPECT_EQ(read, (std::vector<float>{2.0F, 0.5F, 1.0F, 0.75F}));
}

// A frame that draws what the frame before it drew allocates nothing for a batch of several textures with data.
TEST_F(SpriteDataTest, DrawsFramesWithoutAllocating) {
    SpriteBatch batch(std::vector<graphics::Texture>{makeTexture(8), makeTexture(16), makeTexture(32)});
    for (int index = 0; index < 300; ++index) {
        batch.add({.position = {static_cast<float>(index), 0.0F}, .size = {4.0F, 4.0F}});
        batch.setData(static_cast<std::size_t>(index), {.texture = static_cast<std::uint8_t>(index % 3), .values = {static_cast<float>(index)}});
    }
    // clang-format off
    fixture.engine().getScenes().replace(std::make_shared<test::DrawingScene>([&batch](core::Engine& engine) {
        engine.getRenderer2D().beginWorld(Camera{}, {.sort = Renderer::SortMode::Y});
        batch.draw(engine.getRenderer2D());
    }));
    // clang-format on
    fixture.frames(5);
    const test::AllocationTracker tracker;
    fixture.frames(10);
    EXPECT_EQ(tracker.getTotal(), 0U);
}

TEST_F(SpriteDataTest, DrawsBatchesOfSeveralTexturesFromLua) {
    lua("graphics = require('haylen.graphics') graphics2d = require('haylen.graphics2d') small = graphics.newTexture(4, 4) large = graphics.newTexture(16, 16)");
    lua("crowd = graphics2d.newSpriteBatch({small, large}) crowd:add({x = 1}) crowd:add({x = 2, texture = 2, data = {0.5, 1}}) crowd:set(1, {data = {3}})");
    EXPECT_EQ(lua("local first, second = crowd:get(1), crowd:get(2) return first.texture .. ' ' .. first.data[1] .. ' ' .. first.width .. ' ' .. second.texture .. ' ' .. second.data[2] .. ' ' .. second.width"), "1 3.0 4.0 2 1.0 16.0");
    EXPECT_NE(lua("crowd:add({texture = 3})").find("A sprite draws from the texture 3 of its batch, which has 2 textures."), std::string::npos);
    EXPECT_NE(lua("crowd:add({data = {1, 2, 3, 4, 5}})").find("The data of a sprite holds up to 4 numbers, and this one has 5."), std::string::npos);
    EXPECT_NE(lua("graphics2d.newSpriteBatch({small, graphics.newTexture(4, 4, {filter = 'linear'})})").find("The textures of a sprite batch share their filter, wrap and mipmaps"), std::string::npos);

    // clang-format off
    lua(R"(
        collections = require('haylen.collections')
        buffer = collections.newFloatBuffer(6)
        buffer:set(1, 10, 1, 0.5, 20, 2, 0.25)
        scene = require('haylen.scene')
        scene.push({render = function()
            graphics2d.beginScreen()
            crowd:draw()
            graphics2d.drawBatch({small, large}, {{x = 1}, {x = 2, texture = 2, data = {1}}})
            graphics2d.drawBatch({small, large}, buffer, {fields = {'x', 'texture', 'data1'}, width = 4, height = 4})
        end})
    )");
    // clang-format on
    fixture.frames(1);
    EXPECT_EQ(lua("local stats = graphics2d.stats() return stats.sprites .. ' ' .. stats.drawCalls"), "6 1");
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

} // namespace haylen::graphics2d
