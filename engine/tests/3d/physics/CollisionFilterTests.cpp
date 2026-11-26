#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>

#include "3d/physics/jolt/Backend.hpp"
#include "haylen/3d/physics/Body.hpp"
#include "haylen/3d/physics/CollisionCategories.hpp"
#include "haylen/3d/physics/CollisionFilter.hpp"
#include "haylen/3d/physics/Shape.hpp"
#include "haylen/3d/physics/World.hpp"
#include "haylen/3d/physics/WorldSettings.hpp"
#include "haylen/core/Json.hpp"

namespace haylen::physics3d {

class CollisionFilter3DTest : public ::testing::Test {
  protected:
    static constexpr float kStep = 1.0F / 60.0F;
    static constexpr std::uint32_t kGround = 1U << 0;
    static constexpr std::uint32_t kPlayer = 1U << 1;
    static constexpr std::uint32_t kGhost = 1U << 31;

    [[nodiscard]] std::shared_ptr<const Shape> make(Shape::Geometry geometry) const {
        return backend.createShape({.geometry = std::move(geometry)});
    }

    // A floor of the ground category whose top lies at zero.
    void addFloor(World& world) const {
        world.createBody({.type = Body::Type::Static, .shape = make(Shape::Box{.size = {40.0F, 1.0F, 40.0F}}), .position = {0.0F, -0.5F, 0.0F}, .filter = {.category = kGround}});
    }

    static void run(World& world, int steps) {
        for (int step = 0; step < steps; ++step) {
            world.step(kStep);
        }
    }

    jolt::Backend backend;
    std::shared_ptr<const Shape> crate = make(Shape::Box{.size = {1.0F, 1.0F, 1.0F}});
};

TEST_F(CollisionFilter3DTest, CollidesWhenEachCategoryMeetsTheOtherMaskUnlessAGroupDecides) {
    const CollisionFilter ground{.category = kGround};
    const CollisionFilter player{.category = kPlayer, .mask = kGround};
    const CollisionFilter ghost{.category = kGhost, .mask = kPlayer};
    EXPECT_TRUE(ground.canCollide(player));
    EXPECT_FALSE(ghost.canCollide(ground));
    EXPECT_FALSE(player.canCollide(ghost));

    const CollisionFilter ragdoll{.category = kPlayer, .mask = kGround | kPlayer, .group = -3};
    const CollisionFilter magnet{.category = kGhost, .mask = 0, .group = 5};
    EXPECT_FALSE(ragdoll.canCollide(ragdoll));
    EXPECT_TRUE(magnet.canCollide(magnet));
    EXPECT_TRUE(player.canFind(ground));
    EXPECT_FALSE(CollisionFilter{.mask = kPlayer}.canFind(ground));
}

TEST_F(CollisionFilter3DTest, LetsOnlyTheBodiesWhoseFiltersMeetStandOnEachOther) {
    World world({}, backend);
    addFloor(world);
    const Body standing = world.createBody({.shape = crate, .position = {-4.0F, 2.0F, 0.0F}, .filter = {.category = kPlayer, .mask = kGround}});
    const Body falling = world.createBody({.shape = crate, .position = {0.0F, 2.0F, 0.0F}, .filter = {.category = kGhost, .mask = kPlayer}});
    const Body lower = world.createBody({.shape = crate, .position = {4.0F, 0.5F, 0.0F}, .filter = {.category = kPlayer, .group = -1}});
    const Body upper = world.createBody({.shape = crate, .position = {4.0F, 2.0F, 0.0F}, .filter = {.category = kPlayer, .group = -1}});
    const Body held = world.createBody({.shape = crate, .position = {8.0F, 0.5F, 0.0F}, .filter = {.category = kPlayer, .mask = kGround, .group = 2}});
    const Body holder = world.createBody({.shape = crate, .position = {8.0F, 2.0F, 0.0F}, .filter = {.category = kGhost, .mask = 0, .group = 2}});
    run(world, 120);
    EXPECT_GT(standing.getPosition().y, 0.4F);
    EXPECT_LT(falling.getPosition().y, -2.0F);
    EXPECT_NEAR(upper.getPosition().y, lower.getPosition().y, 0.05F);
    EXPECT_GT(holder.getPosition().y, held.getPosition().y + 0.9F);
    EXPECT_EQ(holder.getFilter(), (CollisionFilter{.category = kGhost, .mask = 0, .group = 2}));
}

TEST_F(CollisionFilter3DTest, ChangesTheFilterOfABodyWhileItLives) {
    World world({}, backend);
    addFloor(world);
    Body body = world.createBody({.shape = crate, .position = {0.0F, 0.5F, 0.0F}, .filter = {.category = kPlayer}});
    run(world, 60);
    EXPECT_GT(body.getPosition().y, 0.4F);

    body.setFilter({.category = kPlayer, .mask = kPlayer});
    run(world, 60);
    EXPECT_LT(body.getPosition().y, -1.0F);

    body.setType(Body::Type::Static);
    body.setSensor(true);
    EXPECT_EQ(body.getFilter(), (CollisionFilter{.category = kPlayer, .mask = kPlayer}));
}

TEST_F(CollisionFilter3DTest, NamesTheCategoriesOfHaylenJsonInTheOrderOfTheirBits) {
    CollisionCategories categories;
    categories.readSection(core::Json::parse(R"({"categories": ["ground", "player", "enemy"], "velocitySteps": 8})"));
    EXPECT_EQ(categories.getBit("ground"), 1U);
    EXPECT_EQ(categories.getBit("enemy"), 4U);
    EXPECT_EQ(categories.getNames().size(), 3U);

    WorldSettings settings;
    settings.readSection(core::Json::parse(R"({"categories": ["ground"], "velocitySteps": 8})"));
    EXPECT_EQ(settings.velocitySteps, 8);

    // clang-format off
    const auto refusal = [](const char* section) {
        try {
            CollisionCategories read;
            read.readSection(core::Json::parse(section));
        } catch (const std::invalid_argument& error) {
            return std::string(error.what());
        }

        return std::string();
    };
    // clang-format on
    EXPECT_EQ(refusal(R"({"categories": ["a", "a"]})"), "The category \"a\" appears twice in \"physics3d.categories\" in \"haylen.json\".");
    EXPECT_EQ(refusal(R"({"categories": [""]})"), "Item 1 of \"physics3d.categories\" in \"haylen.json\" must be a name that is not empty.");
    EXPECT_EQ(refusal(R"({"categories": "ground"})"), "The value of \"physics3d.categories\" in \"haylen.json\" must be a list of up to 32 names, such as [\"ground\", \"player\"].");
    try {
        static_cast<void>(categories.getBit("water"));
        FAIL() << "An unknown category had a bit.";
    } catch (const std::invalid_argument& error) {
        EXPECT_EQ(std::string(error.what()), "No 3D collision category is named \"water\". The categories of \"physics3d.categories\" in \"haylen.json\" are \"ground\", \"player\", \"enemy\".");
    }
}

} // namespace haylen::physics3d
