#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "haylen/2d/spatial/OverlapWorld.hpp"
#include "haylen/math/Random.hpp"

namespace haylen::spatial2d {

// A fighter with a body box that the hits of the other team reach, and a punch with a hit box that reaches bodies of the first layer.
class OverlapWorldTest : public ::testing::Test {
  protected:
    static constexpr OverlapWorld::Box kBody{.rect = {-18.0F, -78.0F, 36.0F, 78.0F}, .layers = 1, .depth = 12.0F};
    static constexpr OverlapWorld::Box kHit{.rect = {12.0F, -62.0F, 34.0F, 14.0F}, .kind = 1, .layers = 0, .mask = 1};

    [[nodiscard]] static std::vector<OverlapWorld::Contact> contactsOf(const OverlapWorld& world) {
        std::vector<OverlapWorld::Contact> contacts;
        world.collect(contacts);
        return contacts;
    }
};

TEST_F(OverlapWorldTest, MirrorsBoxesAndReachesOnlyByMask) {
    OverlapWorld world({.kinds = {"body", "hit"}});
    const std::uint32_t standing = world.defineSet(std::vector{kBody});
    const std::uint32_t punching = world.defineSet(std::vector{kBody, kHit});
    const std::uint32_t hero = world.add();
    const std::uint32_t rival = world.add();

    // The hero faces left and punches the rival on its left, whose body never reaches back.
    world.place(hero, {200.0F, 0.0F}, 0.0F, true, punching);
    world.place(rival, {160.0F, 0.0F}, 0.0F, false, standing);
    const std::vector<OverlapWorld::Contact> contacts = contactsOf(world);
    ASSERT_EQ(contacts.size(), 1U);
    EXPECT_EQ(contacts[0].source, hero);
    EXPECT_EQ(contacts[0].sourceBox, 2U);
    EXPECT_EQ(contacts[0].target, rival);
    EXPECT_EQ(contacts[0].targetBox, 1U);
    EXPECT_EQ(contacts[0].overlap, (haylen::math::Rect{154.0F, -62.0F, 24.0F, 14.0F}));

    // Facing right, the punch goes the other way and misses.
    world.place(hero, {200.0F, 0.0F}, 0.0F, false, punching);
    EXPECT_TRUE(contactsOf(world).empty());
}

TEST_F(OverlapWorldTest, MeetsWithinTheSumOfTheDepthReaches) {
    OverlapWorld world({.kinds = {"body", "hit"}});
    const std::uint32_t standing = world.defineSet(std::vector{kBody});
    const std::uint32_t punching = world.defineSet(std::vector{kBody, kHit});
    const std::uint32_t hero = world.add();
    const std::uint32_t rival = world.add();
    world.place(hero, {0.0F, 0.0F}, 400.0F, false, punching);
    world.place(rival, {40.0F, 0.0F}, 412.0F, false, standing);
    EXPECT_EQ(contactsOf(world).size(), 1U);
    world.place(rival, {40.0F, 0.0F}, 412.5F, false, standing);
    EXPECT_TRUE(contactsOf(world).empty());
}

TEST_F(OverlapWorldTest, CountsTouchingEdgesOnlyWhenAsked) {
    const OverlapWorld::Box left{.rect = {0.0F, 0.0F, 10.0F, 10.0F}, .mask = 1};
    const OverlapWorld::Box right{.rect = {10.0F, 0.0F, 10.0F, 10.0F}};
    for (const bool touching : {true, false}) {
        OverlapWorld world({.touching = touching});
        const std::uint32_t first = world.add();
        const std::uint32_t second = world.add();
        world.place(first, {}, 0.0F, false, world.defineSet(std::vector{left}));
        world.place(second, {}, 0.0F, false, world.defineSet(std::vector{right}));
        EXPECT_EQ(contactsOf(world).size(), touching ? 1U : 0U);
    }
}

TEST_F(OverlapWorldTest, RestsAPairForItsTicksAndKeepsTheOrderOfEntities) {
    OverlapWorld world({.kinds = {"body", "hit"}});
    const std::uint32_t punching = world.defineSet(std::vector{kBody, kHit});
    const std::uint32_t first = world.add();
    const std::uint32_t second = world.add();
    const std::uint32_t third = world.add();

    for (const std::uint32_t entity : {first, second, third}) {
        world.place(entity, {static_cast<float>(entity) * 30.0F, 0.0F}, 0.0F, false, punching);
    }

    world.rest(first, second, 2);
    EXPECT_TRUE(world.isResting(first, second));
    world.step();
    EXPECT_TRUE(world.isResting(first, second));
    world.step();
    EXPECT_FALSE(world.isResting(first, second));

    // A removed entity frees its id, and the entity that takes the id comes last in the order of contacts.
    world.remove(first);
    const std::uint32_t fourth = world.add();
    EXPECT_EQ(fourth, first);
    world.place(fourth, {0.0F, 0.0F}, 0.0F, false, punching);
    const std::vector<OverlapWorld::Contact> contacts = contactsOf(world);
    ASSERT_FALSE(contacts.empty());
    EXPECT_EQ(contacts.front().source, second);
    EXPECT_EQ(contacts.back().source, fourth);
    EXPECT_THROW(world.place(99, {}, 0.0F, false, 1), std::out_of_range);
    EXPECT_THROW(world.place(second, {}, 0.0F, false, 9), std::out_of_range);
    EXPECT_THROW((void)world.defineSet(std::vector{OverlapWorld::Box{.rect = {0.0F, 0.0F, 0.0F, 4.0F}}}), std::invalid_argument);
    EXPECT_THROW((void)world.defineSet(std::vector{OverlapWorld::Box{.rect = {0.0F, 0.0F, 4.0F, 4.0F}, .kind = 2}}), std::invalid_argument);
}

TEST_F(OverlapWorldTest, TheHashGridFindsTheSameContactsAsEveryPair) {
    OverlapWorld everyPair({.kinds = {"body", "hit"}});
    OverlapWorld hashed({.kinds = {"body", "hit"}, .cellSize = 128.0F});

    for (OverlapWorld* world : {&everyPair, &hashed}) {
        const std::uint32_t standing = world->defineSet(std::vector{kBody});
        const std::uint32_t punching = world->defineSet(std::vector{kBody, kHit});
        haylen::math::Random places(5);
        for (int entity = 0; entity < 1000; ++entity) {
            const std::uint32_t id = world->add();
            world->place(id, {places.range(0.0F, 2000.0F), places.range(0.0F, 400.0F)}, places.range(0.0F, 60.0F), places.chance(0.5F), places.chance(0.5F) ? standing : punching);
        }
    }

    const std::vector<OverlapWorld::Contact> expected = contactsOf(everyPair);
    const std::vector<OverlapWorld::Contact> found = contactsOf(hashed);
    ASSERT_GT(expected.size(), 10U);
    ASSERT_EQ(found.size(), expected.size());

    for (std::size_t index = 0; index < expected.size(); ++index) {
        EXPECT_EQ(found[index].source, expected[index].source);
        EXPECT_EQ(found[index].target, expected[index].target);
        EXPECT_EQ(found[index].sourceBox, expected[index].sourceBox);
        EXPECT_EQ(found[index].overlap, expected[index].overlap);
    }
}

} // namespace haylen::spatial2d
