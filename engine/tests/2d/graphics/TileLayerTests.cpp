#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/TileLayer.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "support/AllocationTracker.hpp"
#include "support/EngineFixture.hpp"
#include "support/FrameRenderer.hpp"
#include "support/TestFiles.hpp"

namespace haylen::graphics2d {

namespace {

class TileLayerTest : public ::testing::Test {
  protected:
    TileLayerTest() : fixture({{"content/tiles.png", png(64, 32)}, {"content/props.png", png(32, 32)}}) {}

    [[nodiscard]] static std::string png(int width, int height) {
        const std::vector<std::uint8_t> bytes = test::TestFiles::pngImage(width, height, 0xFFFFFFFFU);
        return {bytes.begin(), bytes.end()};
    }

    [[nodiscard]] TileLayer makeLayer(int chunkSize) {
        assets::Manager& assets = fixture.engine().getAssets();
        TileLayer layer({assets.texture("tiles.png"), assets.texture("props.png")}, {.cellSize = {16.0F, 16.0F}, .chunkSize = chunkSize});
        layer.defineFrame(1, {.texture = 0, .source = {0.0F, 0.0F, 16.0F, 16.0F}});
        layer.defineFrame(2, {.texture = 0, .source = {16.0F, 0.0F, 16.0F, 16.0F}});
        layer.defineFrame(3, {.texture = 1, .source = {0.0F, 0.0F, 32.0F, 32.0F}, .offset = {0.0F, -16.0F}});
        return layer;
    }

    test::EngineFixture fixture;
};

} // namespace

TEST_F(TileLayerTest, KeepsSparseChunksAndBakesOnlyChangedOnes) {
    TileLayer layer = makeLayer(4);
    layer.setCell(0, 0, 1);
    layer.setCell(-1, -1, 2);
    layer.setCell(-5, 9, 3);
    EXPECT_EQ(layer.getChunkCount(), 3U);
    EXPECT_EQ(layer.getCell(-1, -1), 2);
    EXPECT_EQ(layer.getCell(-5, 9), 3);
    EXPECT_EQ(layer.getCell(1000, -1000), 0);

    // A rectangle that crosses chunks writes each of its cells, and an empty chunk goes away.
    const std::vector<float> ids{1.0F, 2.0F, 0.0F, 3.0F, 3.0F, 1.0F};
    layer.setCells(2, 2, 3, ids);
    EXPECT_EQ(layer.getCell(4, 2), 0);
    EXPECT_EQ(layer.getCell(4, 3), 1);
    EXPECT_EQ(layer.getChunkCount(), 4U);
    layer.setCell(-5, 9, 0);
    EXPECT_EQ(layer.getChunkCount(), 3U);

    Camera camera;
    camera.anchor = Camera::Anchor::TopLeft;
    camera.position = {-64.0F, -64.0F};
    // clang-format off
    const auto draw = [&](math::Vec2 offset) {
        return test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
            engine.getRenderer2D().beginWorld(camera);
            layer.draw(engine.getRenderer2D(), {}, offset);
        });
    };
    // clang-format on

    // The chunks bake on their first draw into one batch for each texture they use.
    const Renderer::Stats stats = draw({});
    EXPECT_EQ(stats.sprites, 7U);
    EXPECT_EQ(layer.getBatchCount(), 4U);

    // Redefining a frame bakes the chunks that show it, here with every sprite of the chunk that used both textures on one.
    layer.defineFrame(3, {.texture = 0, .source = {32.0F, 0.0F, 16.0F, 16.0F}});
    EXPECT_EQ(draw({}).sprites, 7U);
    EXPECT_EQ(layer.getBatchCount(), 3U);

    // An offset moves the chunks out of view, and nothing outside it draws.
    EXPECT_EQ(draw({100000.0F, 0.0F}).sprites, 0U);

    layer.clear();
    EXPECT_EQ(layer.getChunkCount(), 0U);
    EXPECT_EQ(draw({}).sprites, 0U);
}

TEST_F(TileLayerTest, DrawsOnlyTheChunksInViewWithoutAllocating) {
    TileLayer layer = makeLayer(16);
    std::vector<float> ids(256UL * 256UL, 1.0F);
    layer.setCells(0, 0, 256, ids);
    EXPECT_EQ(layer.getChunkCount(), 256U);

    Camera camera;
    camera.anchor = Camera::Anchor::TopLeft;
    std::size_t inside = 0;
    // clang-format off
    fixture.engine().getScenes().replace(std::make_shared<test::DrawingScene>([&](core::Engine& engine) {
        engine.getRenderer2D().beginWorld(camera);
        const math::Rect view = engine.getRenderer2D().getCanvasBounds();
        inside = 0;
        for (int row = 0; row < 16; ++row) {
            for (int column = 0; column < 16; ++column) {
                inside += view.intersects({static_cast<float>(column) * 256.0F, static_cast<float>(row) * 256.0F, 256.0F, 256.0F}) ? 1U : 0U;
            }
        }
        layer.draw(engine.getRenderer2D());
    }));
    // clang-format on
    fixture.frames(3);
    EXPECT_GT(inside, 0U);
    EXPECT_LT(inside, 256U);
    EXPECT_EQ(fixture.engine().getRenderer2D().getStats().sprites, inside * 256U);
    EXPECT_EQ(layer.getBatchCount(), inside);

    const test::AllocationTracker tracker;
    fixture.frames(5);
    EXPECT_EQ(tracker.getTotal(), 0U);
}

TEST_F(TileLayerTest, DrawsFramesAtTheirSizeOrTheSizeOfTheirSource) {
    TileLayer layer = makeLayer(4);
    layer.defineFrame(4, {.source = {0.0F, 0.0F, 16.0F, 16.0F}, .size = {40.0F, 24.0F}});
    layer.setCell(0, 0, 1);
    layer.setCell(1, 0, 4);
    Camera camera;
    camera.anchor = Camera::Anchor::TopLeft;
    std::vector<math::Vec2> sizes;
    // clang-format off
    test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        engine.getRenderer2D().beginWorld(camera);
        layer.draw(engine.getRenderer2D());
        sizes.clear();
        engine.getRenderer2D().visitDrawn([&sizes](const Renderer::Drawn& drawn) { sizes.push_back(drawn.corners[2] - drawn.corners[0]); });
    });
    // clang-format on
    // The chunk bakes into one batch, which reaches from the 16 unit tile of the first cell to the end of the 40 by 24 unit frame of the second.
    ASSERT_EQ(sizes.size(), 1U);
    EXPECT_EQ(sizes[0], math::Vec2(56.0F, 24.0F));
}

TEST_F(TileLayerTest, RejectsBrokenFramesAndOptions) {
    assets::Manager& assets = fixture.engine().getAssets();
    EXPECT_THROW(TileLayer({}, {}), std::invalid_argument);
    EXPECT_THROW(TileLayer({assets.texture("tiles.png")}, {.chunkSize = 0}), std::invalid_argument);
    EXPECT_THROW(TileLayer({assets.texture("tiles.png")}, {.cellSize = {0.0F, 16.0F}}), std::invalid_argument);

    TileLayer layer = makeLayer(8);
    EXPECT_THROW(layer.defineFrame(0, {}), std::invalid_argument);
    EXPECT_THROW(layer.defineFrame(TileLayer::kMaxFrame + 1, {}), std::invalid_argument);
    EXPECT_THROW(layer.defineFrame(4, {.texture = 2}), std::invalid_argument);
    EXPECT_THROW(layer.setCell(0, 0, 9), std::invalid_argument);
    const std::vector<float> ids{1.0F, 1.0F, 1.0F};
    EXPECT_THROW(layer.setCells(0, 0, 2, ids), std::invalid_argument);
}

TEST_F(TileLayerTest, BuildsLayersFromLua) {
    // clang-format off
    fixture.runLua(R"(
        graphics2d = require('haylen.graphics2d')
        collections = require('haylen.collections')
        assets = require('haylen.assets')
        camera = graphics2d.newCamera()
        camera.anchor = 'topLeft'
        layer = graphics2d.newTileLayer({assets.texture('tiles.png'), assets.texture('props.png')}, {cellWidth = 16, cellHeight = 16, chunkSize = 8})
        layer:defineFrame(1, {source = {0, 0, 16, 16}, width = 16, height = 16})
        layer:defineFrame(2, {texture = 2, source = {0, 0, 32, 32}, offsetY = -16})
        local ids = collections.newFloatBuffer(20, 1)
        ids[3] = 2
        layer:setCells(-2, 0, 10, ids)
        layer:setCell(300, 300, 2)
        require('haylen.scene').push({render = function()
            graphics2d.beginWorld(camera)
            layer:draw({layer = 1}, 32, 0)
        end})
    )");
    // clang-format on
    fixture.frames(1);
    ASSERT_EQ(fixture.engine().getError(), nullptr) << fixture.engine().getError()->what();
    EXPECT_EQ(fixture.lua("return layer:cell(0, 0) .. ' ' .. layer:cell(-2, 1) .. ' ' .. layer:cell(300, 300) .. ' ' .. layer.chunkCount .. ' ' .. layer.batchCount"), "2 1 2 3 3");
    EXPECT_EQ(fixture.engine().getRenderer2D().getStats().sprites, 20U);

    fixture.runLua("layer:clear()");
    EXPECT_EQ(fixture.lua("return layer.chunkCount .. ' ' .. layer:cell(0, 0)"), "0 0");
    EXPECT_NE(fixture.lua("layer:defineFrame(1, {source = {0, 0, 16, 16}, flip = true})").find("Unknown option \"flip\"."), std::string::npos);
    EXPECT_NE(fixture.lua("layer:defineFrame(3, {texture = 0})").find("textures are numbered from one"), std::string::npos);
    EXPECT_NE(fixture.lua("layer:setCell(0, 0, 7)").find("The tile layer has no frame 7."), std::string::npos);
    EXPECT_NE(fixture.lua("graphics2d.newTileLayer({}, {})").find("A tile layer needs at least one texture."), std::string::npos);
    EXPECT_NE(fixture.lua("graphics2d.newTileLayer(assets.texture('tiles.png'), {size = 4})").find("Unknown option \"size\"."), std::string::npos);
}

} // namespace haylen::graphics2d
