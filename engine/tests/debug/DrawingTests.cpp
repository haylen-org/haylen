#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/core/Scene.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/plugins/DebugPlugin.hpp"
#include "support/DrawingScene.hpp"
#include "support/EngineFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::debug {

namespace {

class DebugDrawingTest : public ::testing::Test {
  protected:
    [[nodiscard]] plugins::DebugPlugin& getPlugin() {
        return fixture.engine().getPlugin<plugins::DebugPlugin>();
    }

    void press(input::Key key) {
        platform::Event event;
        event.type = platform::Event::Type::KeyDown;
        event.key = key;
        fixture.engine().handleEvent(event);
    }

    // Draws a texture in a world canvas and another in a screen canvas every frame, and returns the number of quads the last frame drew.
    std::size_t drawFrame(const graphics::Texture& texture) {
        // clang-format off
        fixture.engine().getScenes().replace(std::make_shared<test::DrawingScene>([texture](core::Engine& engine) {
            graphics2d::Camera camera;
            engine.getRenderer2D().beginWorld(camera);
            engine.getRenderer2D().draw({.texture = texture, .position = {10.0F, 10.0F}});
            engine.getRenderer2D().beginScreen();
            engine.getRenderer2D().draw({.texture = texture, .position = {40.0F, 40.0F}});
        }));
        // clang-format on
        fixture.frames(1);
        return fixture.engine().getRenderer2D().getStats().sprites;
    }

    [[nodiscard]] static std::map<std::string, std::string> files() {
        const std::vector<std::uint8_t> image = test::TestFiles::pngImage(8, 8, 0xFFFFFFFFU);
        return {{"content/units/hero.png", std::string(image.begin(), image.end())}};
    }

    test::EngineFixture fixture{files()};
};

} // namespace

TEST_F(DebugDrawingTest, RunsTheDrawersOfTheDrawingsThatAreOnInEveryCanvas) {
    plugins::DebugPlugin& plugin = getPlugin();
    std::vector<graphics2d::Renderer::CanvasKind> kinds;
    core::Connection drawer = plugin.addDrawer("marks", [&kinds](graphics2d::Renderer& renderer) { kinds.push_back(renderer.getCanvasKind()); });
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(4, 4, math::Color::white()));

    (void)drawFrame(texture);
    EXPECT_TRUE(kinds.empty());
    EXPECT_EQ(plugin.getDrawingNames(), (std::vector<std::string>{"bounds", "marks", "physics"}));

    plugin.setDrawing("marks", true);
    (void)drawFrame(texture);
    EXPECT_EQ(std::set(kinds.begin(), kinds.end()), (std::set{graphics2d::Renderer::CanvasKind::World, graphics2d::Renderer::CanvasKind::Screen}));
    EXPECT_EQ(plugin.getDrawings(), (std::vector<std::string>{"marks"}));

    // The key turns every drawing off while any is on, and then every drawing the engine and the app know.
    press(input::Key::F4);
    EXPECT_TRUE(plugin.getDrawings().empty());
    press(input::Key::F4);
    EXPECT_EQ(plugin.getDrawings(), plugin.getDrawingNames());
    plugin.setDrawKey(std::nullopt);
    press(input::Key::F4);
    EXPECT_EQ(plugin.getDrawings().size(), 3U);

    kinds.clear();
    drawer.disconnect();
    (void)drawFrame(texture);
    EXPECT_TRUE(kinds.empty());
    EXPECT_EQ(plugin.getDrawingNames(), (std::vector<std::string>{"bounds", "physics"}));
}

// The bounds outline every textured quad and name each sprite after the path of the texture the assets loaded it from.
TEST_F(DebugDrawingTest, OutlinesSpritesWithTheNamesOfTheirTextures) {
    const graphics::Texture texture = fixture.engine().getAssets().texture("units/hero.png");
    const std::size_t plain = drawFrame(texture);
    EXPECT_EQ(plain, 2U);

    getPlugin().setDrawing("bounds", true);
    const std::size_t outlined = drawFrame(texture);
    EXPECT_GT(outlined, plain + 2U * 4U + 2U * std::string("units/hero.png").size() / 2U);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

// Physics worlds made from Lua draw in world canvases, whose units are theirs, and never in screen canvases.
TEST_F(DebugDrawingTest, DrawsThePhysicsWorldsOfLuaInWorldCanvases) {
    // clang-format off
    fixture.runLua(R"(
        physics2d = require('haylen.physics2d') graphics2d = require('haylen.graphics2d') scene = require('haylen.scene')
        world = physics2d.newWorld({gravity = {0, 0}})
        crate = world:createBody({x = 100, y = 100})
        crate:addBox(40, 40)
        scene.push({render = function()
            graphics2d.beginScreen()
            graphics2d.drawRect({0, 0, 10, 10}, '#FFFFFFFF')
        end})
    )");
    // clang-format on
    fixture.frames(1);
    const std::size_t screenOnly = fixture.engine().getRenderer2D().getStats().sprites;
    getPlugin().setDrawing("physics", true);
    fixture.frames(1);
    EXPECT_EQ(fixture.engine().getRenderer2D().getStats().sprites, screenOnly);

    fixture.runLua("scene.clear() camera = graphics2d.newCamera() scene.push({render = function() graphics2d.beginWorld(camera) graphics2d.drawRect({0, 0, 10, 10}, '#FFFFFFFF') end})");
    fixture.frames(2);
    EXPECT_GE(fixture.engine().getRenderer2D().getStats().sprites, screenOnly + 4U);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

TEST_F(DebugDrawingTest, SwitchesDrawingsFromLua) {
    fixture.runLua("debugging = require('haylen.debug') graphics2d = require('haylen.graphics2d')");
    EXPECT_EQ(fixture.lua("return tostring(debugging.drawing('bounds')) .. ' ' .. #debugging.drawings() .. ' ' .. table.concat(debugging.drawingNames(), ',') .. ' ' .. debugging.drawKey()"), "false 0 bounds,physics f4");
    EXPECT_EQ(fixture.lua("debugging.setDrawing('bounds', true) return tostring(debugging.drawing('bounds')) .. ' ' .. debugging.drawings()[1]"), "true bounds");
    EXPECT_EQ(fixture.lua("debugging.setDrawKey('f6') local key = debugging.drawKey() debugging.setDrawKey(nil) return key .. ' ' .. tostring(debugging.drawKey())"), "f6 nil");

    // clang-format off
    fixture.runLua(R"(
        kinds = {}
        owner = {}
        connection = debugging.addDrawer('paths', function(kind)
            kinds[#kinds + 1] = kind
            graphics2d.drawLine(0, 0, 10, 10, 1, '#FFFF0000')
        end, {owner = owner})
        debugging.setDrawing('paths', true)
        require('haylen.scene').push({render = function() graphics2d.beginScreen() end})
    )");
    // clang-format on
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return kinds[1] .. ' ' .. table.concat(debugging.drawingNames(), ',')"), "screen bounds,paths,physics");
    EXPECT_EQ(fixture.lua("connection:disconnect() return table.concat(debugging.drawingNames(), ',')"), "bounds,physics");
    EXPECT_NE(fixture.lua("debugging.addDrawer('', function() end)").find("a drawing needs a name"), std::string::npos);
    EXPECT_NE(fixture.lua("debugging.addDrawer('x', function() end, {after = 1})").find("Unknown option \"after\""), std::string::npos);
}

TEST(DebugDrawingConfigTest, StartsWithTheDrawingsOfTheAppJson) {
    test::EngineFixture fixture({{"app.json", R"({"name": "Drawings", "identifier": "dev.haylen.drawings", "debug": {"drawings": ["physics", "bounds"]}})"}});
    EXPECT_EQ(fixture.engine().getPlugin<plugins::DebugPlugin>().getDrawings(), (std::vector<std::string>{"bounds", "physics"}));
    EXPECT_THROW((void)core::AppConfig::fromJson(core::Json::parse(R"({"debug": {"drawings": [""]}})")), std::invalid_argument);
}

// The overlay names every scene of the stack after its class, or after its own name, or calls it a scene.
TEST(DebugDrawingConfigTest, NamesScriptedScenesAfterTheirClass) {
    test::EngineFixture fixture;
    fixture.runLua("local haylen = require('haylen') local scene = require('haylen.scene') local Menu = haylen.class('Menu') scene.push(Menu()) scene.push({name = 'pause'}) scene.push({})");
    fixture.frames(1);
    const core::SceneManager& scenes = fixture.engine().getScenes();
    ASSERT_EQ(scenes.size(), 3U);
    EXPECT_EQ(scenes.at(0).getName(), "Menu");
    EXPECT_EQ(scenes.at(1).getName(), "pause");
    EXPECT_EQ(scenes.at(2).getName(), "Scene");
    EXPECT_EQ(core::Scene::stateName(scenes.at(2).getState()), "active");
    EXPECT_EQ(core::Scene::stateFromName("covered"), core::Scene::State::Covered);
}

} // namespace haylen::debug
