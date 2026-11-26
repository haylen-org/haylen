#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/3d/nodes/Node3D.hpp"
#include "haylen/3d/nodes/NodeTree3D.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/math/Mat4.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/Quat.hpp"
#include "haylen/math/Vec3.hpp"
#include "support/AllocationTracker.hpp"

namespace haylen::nodes3d {

class NodeTree3DTest : public ::testing::Test {
  protected:
    static constexpr float kTolerance = 1e-4F;

    [[nodiscard]] static math::Quat turn(math::Vec3 axis, float angle) {
        return math::Quat::fromAxisAngle(axis, angle);
    }

    static void expectNear(math::Vec3 actual, math::Vec3 expected) {
        EXPECT_TRUE(math::Vec3::approximately(actual, expected, kTolerance)) << actual.x << " " << actual.y << " " << actual.z << " against " << expected.x << " " << expected.y << " " << expected.z;
    }

    static void expectNear(const math::Mat4& actual, const math::Mat4& expected) {
        EXPECT_TRUE(math::Mat4::approximately(actual, expected, kTolerance));
    }

    static void expectNear(const math::Quat& actual, const math::Quat& expected) {
        EXPECT_TRUE(math::Quat::approximately(actual, expected, kTolerance)) << actual.x << " " << actual.y << " " << actual.z << " " << actual.w;
    }

    // Builds `roots` roots with `children` children each, every child turned a little, and brings the tree up to date.
    [[nodiscard]] static std::vector<Node3D> grow(NodeTree3D& tree, int roots, int children) {
        std::vector<Node3D> made;
        for (int root = 0; root < roots; ++root) {
            made.push_back(tree.add({.position = {static_cast<float>(root), 0.0F, 0.0F}}));
            for (int child = 0; child < children; ++child) {
                (void)made.back().add({.position = {0.0F, static_cast<float>(child), 0.0F}, .rotation = turn(math::Vec3::up(), 0.1F)});
            }
        }

        tree.update();
        return made;
    }

    // Turns every root on even frames and one root in a hundred on odd frames, then updates.
    static void turnRoots(NodeTree3D& tree, const std::vector<Node3D>& roots, int frame) {
        const std::size_t stride = frame % 2 == 0 ? std::size_t{1} : std::size_t{100};
        for (std::size_t root = static_cast<std::size_t>(frame) % stride; root < roots.size(); root += stride) {
            Node3D turned = roots[root];
            turned.setRotation(turn(math::Vec3::up(), static_cast<float>(frame) * 0.01F));
        }

        tree.update();
    }

    core::JobSystem jobs{[](const std::string& error) { FAIL() << error; }};
};

TEST_F(NodeTree3DTest, ComposesNestedTransformsLikeTheProductOfTheirMatrices) {
    const std::shared_ptr<NodeTree3D> tree = NodeTree3D::create();
    Node3D tank = tree->add({.name = "tank", .position = {10.0F, 0.0F, -4.0F}, .rotation = turn(math::Vec3::up(), 0.5F), .scale = {2.0F, 1.0F, 3.0F}});
    const Node3D turret = tank.add({.name = "turret", .position = {0.0F, 1.5F, 0.0F}, .rotation = turn(math::Vec3::right(), -0.25F), .scale = {0.5F, 0.5F, 0.5F}});
    const Node3D barrel = turret.add({.name = "barrel", .position = {0.0F, 0.0F, -2.0F}, .rotation = turn({0.0F, 0.0F, 1.0F}, 0.1F)});

    // The parent scales its axes differently and its child turns, so only the product of the matrices places the barrel exactly.
    const math::Mat4 expected = math::Mat4::compose({10.0F, 0.0F, -4.0F}, turn(math::Vec3::up(), 0.5F), {2.0F, 1.0F, 3.0F}) * math::Mat4::compose({0.0F, 1.5F, 0.0F}, turn(math::Vec3::right(), -0.25F), {0.5F, 0.5F, 0.5F}) * math::Mat4::compose({0.0F, 0.0F, -2.0F}, turn({0.0F, 0.0F, 1.0F}, 0.1F), {1.0F, 1.0F, 1.0F});
    expectNear(barrel.getWorldMatrix(), expected);
    expectNear(barrel.toWorld({1.0F, 2.0F, 3.0F}), expected.apply({1.0F, 2.0F, 3.0F}));
    expectNear(barrel.toLocal(expected.apply({1.0F, 2.0F, 3.0F})), {1.0F, 2.0F, 3.0F});
    expectNear(barrel.getWorldRotation(), turn(math::Vec3::up(), 0.5F) * turn(math::Vec3::right(), -0.25F) * turn({0.0F, 0.0F, 1.0F}, 0.1F));
    expectNear(barrel.getWorldScale(), {1.0F, 0.5F, 1.5F});
    EXPECT_EQ(barrel.getPath(), "tank/turret/barrel");
    EXPECT_EQ(tree->find("tank/turret/barrel"), barrel);
    EXPECT_EQ(tank.find("turret"), turret);
    EXPECT_FALSE(tree->find("tank/missing").isValid());

    // A read after a change sees the change at once, without an update.
    tank.setPosition({});
    tank.setRotation({});
    tank.setScale({1.0F, 1.0F, 1.0F});
    expectNear(barrel.getWorldPosition(), turret.getPosition() + turn(math::Vec3::right(), -0.25F).rotate(math::Vec3{0.0F, 0.0F, -2.0F} * 0.5F));
}

TEST_F(NodeTree3DTest, RefusesACycleAndKeepsTheTopology) {
    const std::shared_ptr<NodeTree3D> tree = NodeTree3D::create();
    Node3D root = tree->add({.name = "root"});
    const Node3D middle = root.add({.name = "middle", .position = {1.0F, 0.0F, 0.0F}});
    Node3D leaf = middle.add({.name = "leaf", .position = {0.0F, 1.0F, 0.0F}});

    EXPECT_THROW(root.setParent(leaf), std::invalid_argument);
    EXPECT_THROW(root.setParent(root), std::invalid_argument);
    EXPECT_THROW(root.setParent(leaf, true), std::invalid_argument);
    EXPECT_FALSE(root.getParent().isValid());
    EXPECT_EQ(leaf.getParent(), middle);
    EXPECT_EQ(root.getChildren(), std::vector<Node3D>{middle});
    EXPECT_EQ(tree->getRoots(), std::vector<Node3D>{root});
    expectNear(leaf.getWorldPosition(), {1.0F, 1.0F, 0.0F});

    // A legal move after the refusals still works, and the moved node keeps its local transform.
    leaf.setParent(root);
    EXPECT_EQ(root.getChildCount(), 2U);
    expectNear(leaf.getWorldPosition(), {0.0F, 1.0F, 0.0F});
}

TEST_F(NodeTree3DTest, RefusesAStaleHandleAfterItsSlotIsReused) {
    const std::shared_ptr<NodeTree3D> tree = NodeTree3D::create();
    Node3D old = tree->add({.name = "old"});
    const std::uint64_t key = old.getKey();
    old.destroy();
    const Node3D fresh = tree->add({.name = "fresh"});

    EXPECT_FALSE(old.isValid());
    EXPECT_TRUE(fresh.isValid());
    EXPECT_NE(old, fresh);
    EXPECT_FALSE(tree->findByKey(key).isValid());
    EXPECT_EQ(tree->findByKey(fresh.getKey()), fresh);
    try {
        (void)old.getName();
        FAIL() << "A stale handle must throw.";
    } catch (const std::logic_error& error) {
        EXPECT_STREQ(error.what(), "The node was destroyed.");
    }

    const std::shared_ptr<NodeTree3D> other = NodeTree3D::create();
    EXPECT_THROW((void)other->add(fresh, {}), std::logic_error);
    EXPECT_THROW(old.setPosition({1.0F, 0.0F, 0.0F}), std::logic_error);
}

TEST_F(NodeTree3DTest, RecomputesOnlyTheSubtreeOfADeepChange) {
    const std::shared_ptr<NodeTree3D> tree = NodeTree3D::create();

    // Ten roots of a hundred nodes each: a chain of ten under each root, and nine leaves under each link of the chain.
    std::vector<Node3D> links;
    for (int root = 0; root < 10; ++root) {
        Node3D parent = tree->add({.position = {static_cast<float>(root) * 10.0F, 0.0F, 0.0F}});
        for (int depth = 0; depth < 9; ++depth) {
            for (int leaf = 0; leaf < 9; ++leaf) {
                (void)parent.add({.position = {0.0F, 0.0F, static_cast<float>(leaf)}});
            }

            parent = parent.add({.position = {0.0F, 1.0F, 0.0F}});
            links.push_back(parent);
        }

        for (int leaf = 0; leaf < 9; ++leaf) {
            (void)parent.add({.position = {0.0F, 0.0F, static_cast<float>(leaf)}});
        }
    }

    tree->update();
    ASSERT_EQ(tree->size(), 1000U);
    EXPECT_EQ(tree->getUpdatedCount(), 1000U);

    // The sixth link of the fourth root holds its leaves and the three links under it, each with its leaves: 4 links and 36 leaves.
    Node3D deep = links[3 * 9 + 5];
    deep.setX(5.0F);
    tree->update();
    EXPECT_EQ(tree->getUpdatedCount(), 40U);
    expectNear(deep.getWorldPosition(), {35.0F, 6.0F, 0.0F});
    expectNear(links[3 * 9 + 8].getWorldPosition(), {35.0F, 9.0F, 0.0F});
    expectNear(links[2 * 9 + 8].getWorldPosition(), {20.0F, 9.0F, 0.0F});

    // A change inside a changed subtree adds nothing, and a still tree computes nothing.
    deep.setY(2.0F);
    links[3 * 9 + 7].setY(2.0F);
    links[5 * 9].setZ(1.0F);
    tree->update();
    EXPECT_EQ(tree->getUpdatedCount(), 40U + 90U);
    tree->update();
    EXPECT_EQ(tree->getUpdatedCount(), 40U + 90U);
    expectNear(links[3 * 9 + 8].getWorldPosition(), {35.0F, 11.0F, 0.0F});
}

TEST_F(NodeTree3DTest, UpdatesAHundredThousandNodesWithoutAllocating) {
    const std::shared_ptr<NodeTree3D> tree = NodeTree3D::create();
    const std::vector<Node3D> roots = grow(*tree, 1000, 99);
    ASSERT_EQ(tree->size(), 100000U);

    // The first two frames grow the lists that the updates reuse: every root moved, which scans the order, and one root in a hundred, which sorts the changed nodes.
    turnRoots(*tree, roots, 0);
    turnRoots(*tree, roots, 1);

    const test::AllocationTracker tracker;
    for (int frame = 2; frame < 12; ++frame) {
        turnRoots(*tree, roots, frame);
        tree->update();
    }

    EXPECT_EQ(tracker.getTotal(), 0U);
    expectNear(roots[999].getWorldRotation(), math::Quat::fromAxisAngle(math::Vec3::up(), 0.10F));
}

TEST_F(NodeTree3DTest, UpdatesOnTheJobSystemLikeOneThread) {
    const std::shared_ptr<NodeTree3D> serial = NodeTree3D::create();
    const std::shared_ptr<NodeTree3D> parallel = NodeTree3D::create();

    // One root with a deep subtree, which the job system splits below its root, next to many small roots.
    for (const std::shared_ptr<NodeTree3D>& tree : {serial, parallel}) {
        Node3D big = tree->add({.name = "big"});
        for (int branch = 0; branch < 20; ++branch) {
            Node3D part = big.add({.position = {static_cast<float>(branch), 0.0F, 0.0F}, .rotation = turn(math::Vec3::up(), 0.2F), .scale = {1.0F, 2.0F, 1.0F}});
            for (int leaf = 0; leaf < 500; ++leaf) {
                (void)part.add({.position = {0.0F, static_cast<float>(leaf) * 0.01F, 1.0F}, .rotation = turn(math::Vec3::right(), 0.1F)});
            }
        }

        (void)grow(*tree, 200, 20);
    }

    serial->update();
    parallel->update(&jobs);
    EXPECT_EQ(parallel->getUpdatedCount(), serial->getUpdatedCount());

    serial->find("big").setRotation(turn(math::Vec3::up(), 1.0F));
    parallel->find("big").setRotation(turn(math::Vec3::up(), 1.0F));
    serial->update();
    parallel->update(&jobs);
    EXPECT_EQ(parallel->getUpdatedCount(), 10021U);

    std::vector<Node3D> serialNodes{serial->find("big")};
    std::vector<Node3D> parallelNodes{parallel->find("big")};
    for (std::size_t place = 0; place < serialNodes.size(); ++place) {
        EXPECT_EQ(parallelNodes[place].getWorldMatrix(), serialNodes[place].getWorldMatrix());
        for (const Node3D& child : serialNodes[place].getChildren()) {
            serialNodes.push_back(child);
        }

        for (const Node3D& child : parallelNodes[place].getChildren()) {
            parallelNodes.push_back(child);
        }
    }

    EXPECT_EQ(serialNodes.size(), 10021U);
}

TEST_F(NodeTree3DTest, InheritsOnlyTheChosenPartsOfItsParent) {
    const std::shared_ptr<NodeTree3D> tree = NodeTree3D::create();
    const math::Quat quarter = turn(math::Vec3::up(), math::Math::kPi / 2.0F);
    const Node3D parent = tree->add({.position = {10.0F, 0.0F, 0.0F}, .rotation = quarter, .scale = {2.0F, 2.0F, 2.0F}});
    const Node3D placed = parent.add({.position = {1.0F, 0.0F, 0.0F}, .inheritance = {.position = false}});
    const Node3D upright = parent.add({.position = {1.0F, 0.0F, 0.0F}, .inheritance = {.rotation = false}});
    const Node3D unscaled = parent.add({.position = {1.0F, 0.0F, 0.0F}, .scale = {3.0F, 3.0F, 3.0F}, .inheritance = {.scale = false}});
    const Node3D top = parent.add({.position = {1.0F, 2.0F, 3.0F}, .inheritance = {.position = false, .rotation = false, .scale = false}});

    expectNear(placed.getWorldPosition(), {1.0F, 0.0F, 0.0F});
    expectNear(placed.getWorldRotation(), quarter);
    expectNear(placed.getWorldScale(), {2.0F, 2.0F, 2.0F});

    expectNear(upright.getWorldPosition(), {10.0F, 0.0F, -2.0F});
    expectNear(upright.getWorldRotation(), {});
    expectNear(upright.getWorldMatrix().applyVector({1.0F, 0.0F, 0.0F}), {2.0F, 0.0F, 0.0F});

    expectNear(unscaled.getWorldScale(), {3.0F, 3.0F, 3.0F});
    expectNear(unscaled.getWorldMatrix().applyVector({1.0F, 0.0F, 0.0F}), quarter.rotate({3.0F, 0.0F, 0.0F}));

    expectNear(top.getWorldMatrix(), math::Mat4::compose({1.0F, 2.0F, 3.0F}, {}, {1.0F, 1.0F, 1.0F}));
    EXPECT_EQ(top.getParent(), parent);
}

TEST_F(NodeTree3DTest, KeepsItsPlaceInTheWorldUnderANewParentWhenAsked) {
    const std::shared_ptr<NodeTree3D> tree = NodeTree3D::create();
    const Node3D from = tree->add({.position = {5.0F, 0.0F, 0.0F}, .rotation = turn(math::Vec3::up(), 0.7F), .scale = {2.0F, 2.0F, 2.0F}});
    const Node3D to = tree->add({.position = {-3.0F, 4.0F, 1.0F}, .rotation = turn(math::Vec3::right(), -0.4F), .scale = {0.5F, 0.5F, 0.5F}});
    Node3D moving = from.add({.position = {1.0F, 2.0F, 3.0F}, .rotation = turn({0.0F, 0.0F, 1.0F}, 0.3F)});
    const math::Mat4 before = moving.getWorldMatrix();
    const math::Quat rotation = moving.getWorldRotation();

    moving.setParent(to, true);
    EXPECT_EQ(moving.getParent(), to);
    expectNear(moving.getWorldMatrix(), before);
    expectNear(moving.getWorldRotation(), rotation);

    moving.setParent({}, true);
    EXPECT_FALSE(moving.getParent().isValid());
    expectNear(moving.getWorldMatrix(), before);

    // A parent scaled to nothing cannot hold a place in the world, so the move is refused before anything changes.
    const Node3D flat = tree->add({.scale = {1.0F, 0.0F, 1.0F}});
    EXPECT_THROW(moving.setParent(flat, true), std::invalid_argument);
    EXPECT_FALSE(moving.getParent().isValid());
    EXPECT_EQ(flat.getChildCount(), 0U);
    expectNear(moving.getWorldMatrix(), before);
}

TEST_F(NodeTree3DTest, SetsWorldPlacesAndLooksAtATarget) {
    const std::shared_ptr<NodeTree3D> tree = NodeTree3D::create();
    const Node3D parent = tree->add({.position = {2.0F, 0.0F, 0.0F}, .rotation = turn(math::Vec3::up(), 1.1F), .scale = {3.0F, 3.0F, 3.0F}});
    Node3D child = parent.add({});

    child.setWorldPosition({7.0F, 1.0F, -2.0F});
    expectNear(child.getWorldPosition(), {7.0F, 1.0F, -2.0F});
    child.setWorldRotation(turn(math::Vec3::right(), 0.5F));
    expectNear(child.getWorldRotation(), turn(math::Vec3::right(), 0.5F));

    child.lookAt({7.0F, 1.0F, -12.0F});
    expectNear(child.getWorldMatrix().applyVector(math::Vec3::forward()).getNormalized(), math::Vec3::forward());
    child.lookAt({17.0F, 1.0F, -2.0F});
    expectNear(child.getWorldMatrix().applyVector(math::Vec3::forward()).getNormalized(), math::Vec3::right());
    // A target at the place of the node, which the round trip through the parent places only within rounding, is refused the same on every machine, and one a little away is not.
    EXPECT_THROW(child.lookAt({7.0F, 1.0F, -2.0F}), std::invalid_argument);
    EXPECT_THROW(child.lookAt(child.getWorldPosition() + math::Vec3{1e-6F, 0.0F, 0.0F}), std::invalid_argument);
    child.lookAt(child.getWorldPosition() + math::Vec3{0.0F, 0.0F, -0.1F});
    expectNear(child.getWorldMatrix().applyVector(math::Vec3::forward()).getNormalized(), math::Vec3::forward());
    EXPECT_THROW(child.lookAt({7.0F, 11.0F, -2.0F}), std::invalid_argument);
    EXPECT_THROW(child.setRotation({0.0F, 0.0F, 0.0F, 0.0F}), std::invalid_argument);
}

TEST_F(NodeTree3DTest, TellsTheListenersOfANodeUntilTheirOwnerGoes) {
    const std::shared_ptr<NodeTree3D> tree = NodeTree3D::create();
    Node3D root = tree->add({.name = "root"});
    Node3D other = tree->add({.name = "other"});
    Node3D child = root.add({.name = "child"});
    const Node3D off = child.add({.name = "off", .enabled = false});
    const Node3D leaf = child.add({.name = "leaf"});

    std::vector<std::string> heard;
    auto owner = std::make_shared<int>(0);
    for (const NodeEvent3D event : {NodeEvent3D::ParentChanged, NodeEvent3D::ChildAdded, NodeEvent3D::ChildRemoved, NodeEvent3D::EnabledChanged, NodeEvent3D::Destroyed}) {
        for (const Node3D& node : {root, other, child, off, leaf}) {
            // clang-format off
            (void)node.on(event, [&heard, event, name = node.getName()](const Node3D& subject) {
                heard.push_back(name + " " + std::string(core::EnumNames<NodeEvent3D>::name(event)) + " " + subject.getName());
            }, {.owner = owner});
            // clang-format on
        }
    }

    child.setParent(other);
    EXPECT_EQ(heard, (std::vector<std::string>{"child parentChanged child", "root childRemoved child", "other childAdded child"}));

    // Turning a parent off tells the nodes it turns off, and not the node that was off already.
    heard.clear();
    other.setEnabled(false);
    EXPECT_EQ(heard, (std::vector<std::string>{"other enabledChanged other", "child enabledChanged child", "leaf enabledChanged leaf"}));
    EXPECT_FALSE(leaf.isWorldEnabled());
    EXPECT_FALSE(leaf.isWorldVisible());

    // Moving out from under the disabled parent turns the subtree on again.
    heard.clear();
    child.setParent(root);
    EXPECT_EQ(heard, (std::vector<std::string>{"child parentChanged child", "other childRemoved child", "root childAdded child", "child enabledChanged child", "leaf enabledChanged leaf"}));
    EXPECT_TRUE(leaf.isWorldEnabled());

    heard.clear();
    child.destroy();
    EXPECT_EQ(heard, (std::vector<std::string>{"child childRemoved off", "off destroyed off", "child childRemoved leaf", "leaf destroyed leaf", "root childRemoved child", "child destroyed child"}));

    // Listeners end with their owner.
    heard.clear();
    owner.reset();
    (void)root.add({.name = "late"});
    other.setEnabled(true);
    EXPECT_TRUE(heard.empty());
}

TEST_F(NodeTree3DTest, FindsNodesByTagUntilTheyLeaveIt) {
    const std::shared_ptr<NodeTree3D> tree = NodeTree3D::create();
    Node3D first = tree->add({.name = "first", .tags = {"enemy", "flying"}});
    const Node3D second = tree->add({.name = "second", .tags = {"enemy"}});
    Node3D third = tree->add({.name = "third"});
    third.addTag("enemy");
    third.addTag("enemy");

    EXPECT_EQ(tree->findTagged("enemy").size(), 3U);
    EXPECT_TRUE(first.hasTag("flying"));
    EXPECT_EQ(first.getTags(), (std::vector<std::string>{"enemy", "flying"}));
    EXPECT_TRUE(tree->findTagged("missing").empty());

    first.removeTag("enemy");
    first.removeTag("unknown");
    EXPECT_FALSE(first.hasTag("enemy"));
    third.destroy();
    EXPECT_EQ(tree->findTagged("enemy"), std::vector<Node3D>{second});
    EXPECT_EQ(tree->findTagged("flying"), std::vector<Node3D>{first});
}

TEST_F(NodeTree3DTest, ClonesASubtreeWithItsStateEvenIntoItself) {
    const std::shared_ptr<NodeTree3D> tree = NodeTree3D::create();
    Node3D ship = tree->add({.name = "ship", .position = {1.0F, 2.0F, 3.0F}, .layers = 6, .isStatic = true, .renderOrder = 4, .tags = {"vehicle"}, .data = {{"crew", 3}}});
    Node3D wing = ship.add({.name = "wing", .position = {2.0F, 0.0F, 0.0F}, .visible = false, .inheritance = {.scale = false}});

    std::vector<std::pair<std::string, std::string>> copies;
    tree->cloned.connect([&copies](const Node3D& source, const Node3D& copy) { copies.emplace_back(source.getPath(), copy.getPath()); });

    const Node3D copy = ship.clone();
    EXPECT_EQ(tree->getRoots().size(), 2U);
    EXPECT_NE(copy, ship);
    EXPECT_EQ(copy.getName(), "ship");
    EXPECT_EQ(copy.getLayers(), 6U);
    EXPECT_TRUE(copy.isStatic());
    EXPECT_EQ(copy.getRenderOrder(), 4);
    EXPECT_EQ(copy.getData()["crew"], 3);
    EXPECT_TRUE(copy.hasTag("vehicle"));
    const Node3D copiedWing = copy.find("wing");
    ASSERT_TRUE(copiedWing.isValid());
    EXPECT_FALSE(copiedWing.isVisible());
    EXPECT_FALSE(copiedWing.getInheritance().scale);
    EXPECT_EQ(copies, (std::vector<std::pair<std::string, std::string>>{{"ship", "ship"}, {"ship/wing", "ship/wing"}}));

    // A copy under a node of the subtree holds the subtree as it was before the copy.
    const Node3D nested = tree->clone(ship, wing);
    EXPECT_EQ(nested.getParent(), wing);
    EXPECT_EQ(nested.getChildCount(), 1U);
    EXPECT_EQ(nested.find("wing").getChildCount(), 0U);
    expectNear(nested.getWorldPosition(), {4.0F, 4.0F, 6.0F});
    EXPECT_EQ(tree->size(), 6U);
}

TEST_F(NodeTree3DTest, MovesAndReadsManyNodesInOneCall) {
    const std::shared_ptr<NodeTree3D> tree = NodeTree3D::create();
    const Node3D parent = tree->add({.position = {0.0F, 10.0F, 0.0F}});
    std::vector<Node3D> nodes{parent.add({}), parent.add({}), tree->add({})};
    const math::Quat quarter = turn(math::Vec3::up(), math::Math::kPi / 2.0F);
    const std::vector<float> values{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 2.0F, 0.0F, 2.0F, 0.0F, quarter.x, quarter.y, quarter.z, quarter.w, 0.0F, 0.0F, 3.0F, 0.0F, 0.0F, 0.0F, 1.0F};
    tree->writeTransforms(nodes, values);

    std::vector<float> read(nodes.size() * NodeTree3D::kTransformFloats);
    tree->readTransforms(nodes, read);
    EXPECT_FLOAT_EQ(read[0], 1.0F);
    EXPECT_FLOAT_EQ(read[1], 10.0F);
    expectNear(math::Quat{read[3], read[4], read[5], read[6]}, {});
    EXPECT_FLOAT_EQ(read[8], 12.0F);
    expectNear(math::Quat{read[10], read[11], read[12], read[13]}, quarter);
    EXPECT_FLOAT_EQ(read[16], 3.0F);

    // A list with a node that is gone moves no node at all, and neither does a rotation without a length.
    Node3D gone = nodes[2];
    gone.destroy();
    std::vector<float> moved = values;
    moved[0] = 50.0F;
    EXPECT_THROW(tree->writeTransforms(nodes, moved), std::logic_error);
    EXPECT_FLOAT_EQ(nodes[0].getX(), 1.0F);
    nodes.pop_back();
    std::fill(moved.begin() + 10, moved.begin() + 14, 0.0F);
    EXPECT_THROW(tree->writeTransforms(nodes, moved), std::invalid_argument);
    EXPECT_FLOAT_EQ(nodes[0].getX(), 1.0F);
    EXPECT_THROW(tree->readTransforms(nodes, std::span<float>(read).first(13)), std::invalid_argument);
}

TEST_F(NodeTree3DTest, PassesVisibilityAndTheEnabledStateToDescendants) {
    const std::shared_ptr<NodeTree3D> tree = NodeTree3D::create();
    Node3D root = tree->add({.layers = 0b101U, .frustumCulling = false});
    const Node3D child = root.add({});
    EXPECT_TRUE(child.isWorldVisible());
    EXPECT_EQ(root.getLayers(), 0b101U);
    EXPECT_FALSE(root.hasFrustumCulling());

    root.setVisible(false);
    EXPECT_FALSE(child.isWorldVisible());
    EXPECT_TRUE(child.isWorldEnabled());
    root.setVisible(true);
    root.setEnabled(false);
    EXPECT_FALSE(child.isWorldVisible());
    EXPECT_FALSE(child.isWorldEnabled());
    EXPECT_TRUE(child.isEnabled());
}

} // namespace haylen::nodes3d
