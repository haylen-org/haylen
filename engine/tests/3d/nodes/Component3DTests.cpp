#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/3d/nodes/Component3D.hpp"
#include "haylen/3d/nodes/ComponentRegistry3D.hpp"
#include "haylen/3d/nodes/ComponentType3D.hpp"
#include "haylen/3d/nodes/NodeTree3D.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/math/Quat.hpp"
#include "haylen/math/Vec3.hpp"
#include "haylen/plugins/Nodes3DPlugin.hpp"
#include "support/EngineFixture.hpp"
#include "support/SpinnerComponent3D.hpp"
#include "support/TestApplication.hpp"

namespace haylen::nodes3d {

class Component3DTest : public ::testing::Test {
  protected:
    static void expectTurned(const Node3D& node, float angle) {
        EXPECT_TRUE(math::Quat::approximately(node.getRotation(), math::Quat::fromAxisAngle(math::Vec3::up(), angle), 1e-4F)) << node.getName();
    }

    std::shared_ptr<const ComponentType3D> spinner = std::make_shared<test::SpinnerComponent3D>();
};

TEST_F(Component3DTest, RegistersEachTypeOnceByName) {
    ComponentRegistry3D registry;
    registry.add(spinner);
    EXPECT_EQ(registry.find("spinner"), spinner);
    EXPECT_EQ(registry.find("missing"), nullptr);
    EXPECT_THROW(registry.add(std::make_shared<test::SpinnerComponent3D>()), std::invalid_argument);
    EXPECT_EQ(registry.getTypes().size(), 1U);
    EXPECT_TRUE(spinner->runsIn(ComponentType3D::Phase::Update));
    EXPECT_FALSE(spinner->runsIn(ComponentType3D::Phase::LateUpdate));
    ASSERT_NE(spinner->findProperty("axis"), nullptr);
    EXPECT_EQ(spinner->findProperty("axis")->kind, ComponentType3D::Kind::Vec3);
}

TEST_F(Component3DTest, RunsComponentsInBulkAndRestsThemWhileTheirNodeIsOff) {
    const std::shared_ptr<NodeTree3D> tree = NodeTree3D::create();
    Node3D group = tree->add({.name = "group"});
    const Node3D fast = group.add({.name = "fast"});
    const Node3D slow = group.add({.name = "slow"});
    const Node3D bare = tree->add({.name = "bare"});
    Component3D fastSpin = fast.addComponent(spinner, {{"speed", 2.0F}});
    (void)slow.addComponent(spinner);
    EXPECT_THROW((void)fast.addComponent(spinner), std::invalid_argument);
    EXPECT_THROW((void)bare.addComponent(spinner, {{"speed", "fast"}}), std::invalid_argument);
    EXPECT_FALSE(bare.findComponent(spinner).isValid());
    EXPECT_EQ(tree->getComponents().size(), 2U);

    tree->getComponents().run(ComponentType3D::Phase::Update, 0.5F);
    tree->getComponents().run(ComponentType3D::Phase::LateUpdate, 0.5F);
    expectTurned(fast, 1.0F);
    expectTurned(slow, 0.5F);

    // The components of a node that is off rest, and wake when it turns on again.
    group.setEnabled(false);
    tree->getComponents().run(ComponentType3D::Phase::Update, 0.5F);
    expectTurned(fast, 1.0F);
    group.setEnabled(true);
    fastSpin.set("speed", 4.0F);
    EXPECT_EQ(fastSpin.get("speed"), 4.0F);
    tree->getComponents().run(ComponentType3D::Phase::Update, 0.25F);
    expectTurned(fast, 2.0F);
    expectTurned(slow, 0.75F);
    EXPECT_THROW(fastSpin.set("axis", 1), std::invalid_argument);
}

TEST_F(Component3DTest, GoesWithItsNodeAndCopiesWithAClone) {
    const std::shared_ptr<NodeTree3D> tree = NodeTree3D::create();
    Node3D first = tree->add({.name = "first"});
    Node3D second = tree->add({.name = "second"});
    (void)first.addComponent(spinner, {{"speed", 3.0F}, {"axis", {1.0F, 0.0F, 0.0F}}});
    (void)second.addComponent(spinner);

    // The last component takes the place of one that goes, and keeps its own state.
    first.destroy();
    EXPECT_EQ(tree->getComponents().size(), 1U);
    EXPECT_EQ(second.findComponent(spinner).get("speed"), 1.0F);

    Node3D third = tree->add({.name = "third"});
    (void)third.addComponent(spinner, {{"speed", 5.0F}});
    const Node3D copy = third.clone();
    EXPECT_EQ(copy.findComponent(spinner).get("speed"), 5.0F);
    EXPECT_EQ(tree->getComponents().size(), 3U);

    // A node saves its components by type, and another node loads them through a registry.
    const core::Json saved = tree->getComponents().save(third);
    EXPECT_EQ(saved, (core::Json{{"spinner", {{"speed", 5.0F}, {"axis", {0.0F, 1.0F, 0.0F}}}}}));
    ComponentRegistry3D registry;
    registry.add(spinner);
    const Node3D loaded = tree->add({.name = "loaded"});
    tree->getComponents().load(loaded, saved, registry);
    EXPECT_EQ(loaded.getComponents().size(), 1U);
    EXPECT_THROW(tree->getComponents().load(tree->add({}), {{"missing", core::Json::object()}}, registry), std::invalid_argument);

    Component3D removed = copy.findComponent(spinner);
    removed.remove();
    EXPECT_FALSE(removed.isValid());
    EXPECT_EQ(tree->getComponents().size(), 3U);
}

TEST_F(Component3DTest, RunsInThePhasesOfThePluginByTheProcessModeOfTheTree) {
    // clang-format off
    test::EngineFixture app{{}, std::make_unique<test::TestApplication>([this](core::Engine& engine) {
        engine.getPlugin<plugins::Nodes3DPlugin>().getComponentTypes().add(spinner);
    })};
    // clang-format on
    plugins::Nodes3DPlugin& plugin = app.engine().getPlugin<plugins::Nodes3DPlugin>();
    const std::shared_ptr<NodeTree3D> tree = plugin.createTree();
    const Node3D node = tree->add({.name = "wheel"});
    const Node3D child = node.add({.position = {1.0F, 0.0F, 0.0F}});
    (void)node.addComponent(plugin.getComponentTypes().find("spinner"), {{"speed", 6.0F}});

    app.frames(3, 1.0 / 60.0);
    expectTurned(node, 0.3F);

    // The late update of the plugin brings the tree up to date before anything draws.
    EXPECT_EQ(tree->getUpdatedCount(), 2U);

    app.engine().setPaused(true);
    app.frames(3, 1.0 / 60.0);
    expectTurned(node, 0.3F);
    tree->setProcessMode(core::ProcessMode::Always);
    app.frames(3, 1.0 / 60.0);
    expectTurned(node, 0.6F);
    EXPECT_TRUE(child.isWorldEnabled());
}

} // namespace haylen::nodes3d
