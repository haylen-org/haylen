#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/nodes/Node.hpp"
#include "haylen/2d/nodes/NodeSerializer.hpp"
#include "haylen/2d/nodes/NodeTree.hpp"
#include "haylen/2d/nodes/Prefab.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/math/Transform2D.hpp"
#include "support/EngineFixture.hpp"
#include "support/FrameRenderer.hpp"
#include "support/TestFiles.hpp"

namespace haylen::nodes2d {

class NodeTreeTest : public ::testing::Test {
  protected:
    static void expectNear(math::Vec2 actual, math::Vec2 expected) {
        EXPECT_NEAR(actual.x, expected.x, 1.0e-3F);
        EXPECT_NEAR(actual.y, expected.y, 1.0e-3F);
    }

    [[nodiscard]] static std::string png(int width, int height) {
        const std::vector<std::uint8_t> bytes = test::TestFiles::pngImage(width, height, 0xFFFFFFFFU);
        return {bytes.begin(), bytes.end()};
    }
};

class NodeSerializerTest : public NodeTreeTest {};

class Nodes2DLuaTest : public NodeTreeTest {};

TEST_F(NodeTreeTest, ComposesNestedTransformsLikeTheirProduct) {
    const std::shared_ptr<NodeTree> tree = NodeTree::create();
    Node tank = tree->add({.name = "tank", .position = {100.0F, 50.0F}, .rotation = 0.5F, .scale = {2.0F, 1.5F}});
    Node turret = tank.add({.name = "turret", .position = {10.0F, -4.0F}, .rotation = -0.25F, .scale = {0.5F, 0.5F}});
    const Node barrel = turret.add({.name = "barrel", .position = {8.0F, 0.0F}, .rotation = 0.1F});
    tree->update(0.0F);

    const math::Transform2D expected = math::Transform2D::compose({100.0F, 50.0F}, 0.5F, {2.0F, 1.5F}) * math::Transform2D::compose({10.0F, -4.0F}, -0.25F, {0.5F, 0.5F}) * math::Transform2D::compose({8.0F, 0.0F}, 0.1F, {1.0F, 1.0F});
    expectNear(barrel.toWorld({3.0F, 2.0F}), expected.apply({3.0F, 2.0F}));
    expectNear(barrel.toLocal(expected.apply({3.0F, 2.0F})), {3.0F, 2.0F});
    EXPECT_EQ(barrel.getPath(), "tank/turret/barrel");
    EXPECT_EQ(tree->find("tank/turret/barrel"), barrel);
    EXPECT_EQ(tank.find("turret"), turret);
    EXPECT_FALSE(tree->find("tank/missing").isValid());

    // Moving the root moves every descendant at the next update, and the local state of the children stays.
    tank.setPosition({0.0F, 0.0F});
    tank.setRotation(0.0F);
    tank.setScale({1.0F, 1.0F});
    tree->update(0.0F);
    const math::Transform2D moved = math::Transform2D::compose({10.0F, -4.0F}, -0.25F, {0.5F, 0.5F}) * math::Transform2D::compose({8.0F, 0.0F}, 0.1F, {1.0F, 1.0F});
    expectNear(barrel.toWorld({}), moved.apply({}));
    expectNear(turret.getPosition(), {10.0F, -4.0F});

    // A node moved under another parent keeps its local transform.
    const Node other = tree->add({.name = "other", .position = {500.0F, 0.0F}});
    turret.setParent(other);
    tree->update(0.0F);
    expectNear(turret.toWorld({}), {510.0F, -4.0F});
    EXPECT_EQ(turret.getPath(), "other/turret");
    EXPECT_EQ(tank.getChildCount(), 0U);
    EXPECT_THROW(other.getTree().find("other").setParent(barrel), std::invalid_argument);
}

TEST_F(NodeTreeTest, InheritsColorsVisibilityAndLayers) {
    const std::shared_ptr<NodeTree> tree = NodeTree::create();
    Node root = tree->add({.color = {1.0F, 0.5F, 1.0F, 0.5F}, .layer = 2});
    const Node child = root.add({.color = {0.5F, 1.0F, 1.0F, 1.0F}, .layer = 3});
    tree->update(0.0F);
    EXPECT_EQ(child.getWorldColor(), (math::Color{0.5F, 0.5F, 1.0F, 0.5F}));
    EXPECT_EQ(child.getWorldLayer(), 5);
    EXPECT_TRUE(child.isWorldVisible());

    root.setVisible(false);
    tree->update(0.0F);
    EXPECT_FALSE(child.isWorldVisible());
    EXPECT_TRUE(child.isVisible());
}

TEST_F(NodeTreeTest, InvalidatesHandlesOfDestroyedNodesAndOfGoneTrees) {
    std::shared_ptr<NodeTree> tree = NodeTree::create();
    Node parent = tree->add({.name = "parent"});
    const Node child = parent.add({.name = "child"});
    const Node grandchild = child.add({.name = "grandchild"});

    std::vector<std::string> gone;
    tree->destroyed.connect([&gone](const Node& node) { gone.push_back(node.getName()); });
    bool watched = false;
    (void)tree->onDestroyed(child, [&watched, &child](const Node& node) { watched = node == child; });
    parent.destroy();

    EXPECT_EQ(gone, (std::vector<std::string>{"grandchild", "child", "parent"}));
    EXPECT_TRUE(watched);
    EXPECT_FALSE(child.isValid());
    EXPECT_FALSE(grandchild.isValid());
    EXPECT_EQ(tree->size(), 0U);
    EXPECT_THROW((void)child.getName(), std::logic_error);

    // A new node takes the slot of a destroyed one with a new generation, so old handles stay invalid.
    const Node reused = tree->add({.name = "reused"});
    EXPECT_TRUE(reused.isValid());
    EXPECT_FALSE(grandchild.isValid());
    EXPECT_NE(reused.getKey(), grandchild.getKey());

    tree.reset();
    EXPECT_FALSE(reused.isValid());
    try {
        (void)reused.getPosition();
        FAIL();
    } catch (const std::logic_error& error) {
        EXPECT_STREQ(error.what(), "The tree of the node is gone.");
    }
}

TEST_F(NodeTreeTest, MovesManyNodesFromBuffersAndUpdatesLargeTreesInParallel) {
    test::EngineFixture fixture;
    const std::shared_ptr<NodeTree> tree = NodeTree::create();
    std::vector<Node> nodes;
    for (int index = 0; index < 6000; ++index) {
        const Node root = tree->add({.position = {static_cast<float>(index), 0.0F}});
        nodes.push_back(root.add({.position = {0.0F, 1.0F}}));
    }
    std::vector<float> values(nodes.size() * 3);
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        values[index * 3] = 2.0F;
        values[index * 3 + 1] = 3.0F;
        values[index * 3 + 2] = 0.0F;
    }
    tree->writeTransforms(nodes, values);
    tree->update(0.0F, 1.0F, &fixture.engine().getJobs());

    std::vector<float> world(nodes.size() * 3);
    tree->readTransforms(nodes, world);
    EXPECT_FLOAT_EQ(world[0], 2.0F);
    EXPECT_FLOAT_EQ(world[(nodes.size() - 1) * 3], 5999.0F + 2.0F);
    EXPECT_FLOAT_EQ(world[(nodes.size() - 1) * 3 + 1], 3.0F);
    EXPECT_THROW(tree->writeTransforms(nodes, std::span<const float>(values).first(3)), std::invalid_argument);
}

TEST_F(NodeTreeTest, DrawsVisibleNodesAndCullsTheOthers) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(16, 16, math::Color::white()));
    const std::shared_ptr<NodeTree> tree = NodeTree::create();
    Node row = tree->add({});
    for (int index = 0; index < 10; ++index) {
        row.add({.position = {static_cast<float>(index) * 400.0F, 0.0F}, .content = {.value = graphics2d::Sprite{.texture = texture}}});
    }
    Node hidden = tree->add({.visible = false});
    hidden.add({.content = {.value = graphics2d::Sprite{.texture = texture}}});

    graphics2d::Camera camera;
    camera.anchor = graphics2d::Camera::Anchor::TopLeft;
    std::size_t inside = 0;
    // clang-format off
    const graphics2d::Renderer::Stats stats = test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        engine.getRenderer2D().beginWorld(camera);
        const math::Rect view = engine.getRenderer2D().getCanvasBounds();
        for (int index = 0; index < 10; ++index) {
            inside += view.intersects(math::Rect::fromCenter({static_cast<float>(index) * 400.0F, 0.0F}, {16.0F, 16.0F})) ? 1U : 0U;
        }
        tree->draw(engine.getRenderer2D());
    });
    // clang-format on

    EXPECT_GT(inside, 0U);
    EXPECT_LT(inside, 10U);
    EXPECT_EQ(tree->getDrawnCount(), inside);
    EXPECT_EQ(tree->getCulledCount(), 10U - inside);
    EXPECT_EQ(stats.sprites, inside);
    EXPECT_EQ(stats.drawCalls, 1U);
}

TEST_F(NodeTreeTest, FollowsBodiesBetweenSteps) {
    physics2d::World world({.gravity = {0.0F, 0.0F}, .interpolate = true});
    physics2d::Body body = world.createBody({.type = physics2d::Body::Type::Kinematic, .position = {0.0F, 0.0F}, .velocity = {600.0F, 0.0F}});
    world.step(0.1F);
    world.step(0.1F);

    const std::shared_ptr<NodeTree> tree = NodeTree::create();
    const Node parent = tree->add({.position = {100.0F, 0.0F}});
    Node follower = parent.add({});
    follower.follow(body);
    tree->update(0.0F, 0.5F);

    // Halfway between the steps the body is at 90 in the world, so 10 to the left of its parent.
    expectNear(follower.toWorld({}), {90.0F, 0.0F});
    expectNear(follower.getPosition(), {-10.0F, 0.0F});
    EXPECT_EQ(follower.getFollowed(), body);

    follower.follow({});
    world.step(0.1F);
    tree->update(0.0F, 1.0F);
    expectNear(follower.toWorld({}), {90.0F, 0.0F});
}

TEST_F(NodeTreeTest, RejectsContentItCannotDraw) {
    const std::shared_ptr<NodeTree> tree = NodeTree::create();
    try {
        (void)tree->add({.content = {.value = graphics2d::Sprite{}}});
        FAIL();
    } catch (const std::invalid_argument& error) {
        EXPECT_STREQ(error.what(), "A sprite node needs a texture.");
    }
    EXPECT_THROW((void)tree->add({.content = {.value = NodeContent::Text{.content = "Hello"}}}), std::invalid_argument);
    EXPECT_THROW((void)tree->add({.content = {.value = std::shared_ptr<particles2d::Emitter>()}}), std::invalid_argument);
    EXPECT_EQ(tree->size(), 0U);
}

TEST_F(NodeSerializerTest, InstantiatesPrefabsAndSavesSubtrees) {
    test::EngineFixture fixture({{"content/units/tank.png", png(32, 16)}});
    core::Engine& engine = fixture.engine();
    // clang-format off
    const NodeSerializer::Assets assets{
        .texture = [&engine](const std::string& path, graphics::Texture::Options options) { return engine.getAssets().texture(path, options); },
        .font = [&engine](const std::string& path) { return path.empty() ? engine.getDefaultFont() : engine.getAssets().font(path); },
        .particles = {},
    };
    // clang-format on
    const Prefab prefab = Prefab::parse(core::Json::parse(R"({
        "name": "tank", "x": 10, "color": "#FF808080",
        "sprite": {"texture": "units/tank.png", "filter": "linear", "source": [0, 0, 16, 16], "pivotY": 1},
        "children": [
            {"name": "label", "y": -20, "text": {"text": "Tank", "size": 18, "anchor": [0.5, 1]}},
            {"name": "turret", "rotation": 0.5, "layer": 1}
        ]
    })"));

    const std::shared_ptr<NodeTree> tree = NodeTree::create();
    const Node tank = prefab.instantiate(*tree, {}, core::Json::parse(R"({"x": 300, "y": 40, "name": "first"})"), assets);
    EXPECT_EQ(tank.getName(), "first");
    expectNear(tank.getPosition(), {300.0F, 40.0F});
    EXPECT_EQ(tank.getSprite().texture.getOptions().filter, graphics::Texture::Filter::Linear);
    EXPECT_EQ(tank.getSprite().source, (math::Rect{0.0F, 0.0F, 16.0F, 16.0F}));
    EXPECT_EQ(tree->find("first/label").getContent().value.index(), 2U);
    EXPECT_FLOAT_EQ(tree->find("first/turret").getRotation(), 0.5F);

    const core::Json saved = NodeSerializer::save(tank, engine.getDefaultFont());
    EXPECT_EQ(saved["sprite"]["texture"], "units/tank.png");
    EXPECT_FALSE(saved["children"][0]["text"].contains("font"));
    EXPECT_FALSE(saved.contains("visible"));
    const Node copy = NodeSerializer::load(*tree, {}, saved, assets);
    EXPECT_EQ(NodeSerializer::save(copy, engine.getDefaultFont()), saved);

    const graphics::Texture made = engine.getGraphics().createTexture(graphics::Image(4, 4));
    const Node custom = tree->add({.name = "custom", .content = {.value = graphics2d::Sprite{.texture = made}}});
    EXPECT_THROW((void)NodeSerializer::save(custom, engine.getDefaultFont()), std::invalid_argument);
}

TEST_F(NodeSerializerTest, NamesTheNodeAndTheKeyOfBadDescriptions) {
    // clang-format off
    const auto message = [](const char* text) {
        try {
            NodeSerializer::validate(core::Json::parse(text));
        } catch (const std::invalid_argument& error) {
            return std::string(error.what());
        }
        return std::string();
    };
    // clang-format on
    EXPECT_EQ(message(R"({"name": "tank", "children": [{"name": "turret", "speed": 4}]})"), "The key \"speed\" of the node \"tank/turret\" is unknown.");
    EXPECT_EQ(message(R"({"name": "tank", "x": "left"})"), "The key \"x\" of the node \"tank\" must be a number.");
    EXPECT_EQ(message(R"({"name": "tank", "sprite": {"texture": "a.png", "color": 3}})"), "The key \"color\" of the sprite of the node \"tank\" must be a color such as \"#FFFFFFFF\".");
    EXPECT_EQ(message(R"({"name": "tank", "sprite": {}})"), "The sprite of the node \"tank\" needs the path of its texture.");
    EXPECT_EQ(message(R"({"name": "tank", "sprite": {"texture": "a.png"}, "particles": "fire.particles"})"), "The node \"tank\" shows more than one of \"sprite\", \"text\", \"frame\" and \"particles\".");
    EXPECT_EQ(message(R"({"name": "tank", "frame": {"texture": "a.png", "fill": "wrap"}})"), "The key \"fill\" of the frame of the node \"tank\" must be \"stretch\" or \"tile\".");
    EXPECT_EQ(message(R"([1, 2])"), "The description of the node \"\" must be an object.");
}

TEST_F(Nodes2DLuaTest, BuildsDrawsAndDestroysTreesFromLua) {
    test::EngineFixture fixture({{"content/units/tank.png", png(32, 32)}, {"content/units/tank.prefab", R"({"name": "tank", "sprite": {"texture": "units/tank.png"}, "children": [{"name": "turret", "x": 8}]})"}});
    // clang-format off
    fixture.runLua(R"(
        assets = require('haylen.assets')
        collections = require('haylen.collections')
        graphics2d = require('haylen.graphics2d')
        nodes2d = require('haylen.nodes2d')
        tree = nodes2d.newTree()
        hull = tree:add({name = 'hull', x = 100, y = 50, rotation = math.pi / 2, sprite = graphics2d.newSprite(assets.texture('units/tank.png')), data = {health = 3}})
        gun = hull:add({name = 'gun', x = 10, children = {{name = 'tip', x = 5, text = 'Bang'}}})
        tree:update(0)
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("local x, y = gun:toWorld(0, 0) return string.format('%.1f %.1f', x, y)"), "100.0 60.0");
    EXPECT_EQ(fixture.lua("local tip = tree:find('hull/gun/tip') return tip.path .. ' ' .. tip.text .. ' ' .. tip.content .. ' ' .. tostring(tip.parent == gun)"), "hull/gun/tip Bang text true");
    EXPECT_EQ(fixture.lua("return hull.data.health .. ' ' .. hull.childCount .. ' ' .. #tree:roots() .. ' ' .. tree.size"), "3 1 1 3");
    EXPECT_EQ(fixture.lua("hull.x = 200 tree:update(0) local x = gun:toWorld(0, 0) return string.format('%.1f', x)"), "200.0");

    // A prefab from the package instantiates with overrides, and a tank saves back to a description.
    EXPECT_EQ(fixture.lua("local tank = tree:instantiate(assets.load('units/tank.prefab'), {x = 40}) return tank.name .. ' ' .. tank.x .. ' ' .. tank:find('turret').x"), "tank 40.0 8.0");
    EXPECT_EQ(fixture.lua("local saved = nodes2d.save(tree:find('tank')) return saved.sprite.texture .. ' ' .. saved.children[1].name"), "units/tank.png turret");
    EXPECT_EQ(fixture.lua("local copy = tree:load({name = 'copy', children = {{name = 'part'}}}) return copy:find('part').path"), "copy/part");

    // Destroyed listeners run before the nodes go, and the node handles turn invalid.
    // clang-format off
    EXPECT_EQ(fixture.lua(R"(
        local heard = {}
        local owner = {}
        gun:on('destroyed', function(node) heard[#heard + 1] = node.name end, {owner = owner})
        tree:find('hull/gun/tip'):on('destroyed', function(node) heard[#heard + 1] = node.name end)
        hull:destroy()
        return table.concat(heard, ',') .. ' ' .. tostring(gun.valid) .. ' ' .. tree.size
    )"), "tip,gun false 4");
    // clang-format on
    EXPECT_EQ(fixture.lua("local ok, message = pcall(function() return gun.x end) return message:match('The node.*')"), "The node was destroyed.");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(tree.add, tree, {speed = 2}) return message:match('Unknown option.*')"), "Unknown option \"speed\".");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(tree.add, tree, {sprite = graphics2d.newSprite(assets.texture('units/tank.png')), text = 'Hi'}) return message"), "A node shows one of \"sprite\", \"text\", \"frame\" and \"particles\".");

    // Many nodes move from a float buffer in one call.
    // clang-format off
    EXPECT_EQ(fixture.lua(R"(
        local list = {}
        for index = 1, 10 do list[index] = tree:add({}) end
        local values = collections.newFloatBuffer(30)
        for index = 1, 10 do values[index * 3 - 2] = index values[index * 3 - 1] = -index end
        tree:writeTransforms(list, values)
        tree:update(0)
        local world = collections.newFloatBuffer(30)
        tree:readTransforms(list, world)
        return world[28] .. ' ' .. world[29]
    )"), "10.0 -10.0");
    // clang-format on
}

TEST_F(Nodes2DLuaTest, TweensNodesNatively) {
    test::EngineFixture fixture;
    fixture.runLua("nodes2d = require('haylen.nodes2d') tween = require('haylen.tween') tree = nodes2d.newTree() node = tree:add({}) tween.to(node, 0.5, {x = 100, color = '#FF000000'})");
    fixture.frames(60);
    EXPECT_EQ(fixture.lua("return string.format('%.1f %.1f', node.x, node.color.r)"), "100.0 0.0");
}

} // namespace haylen::nodes2d
