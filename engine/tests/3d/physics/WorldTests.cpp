#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

#include "3d/physics/jolt/Backend.hpp"
#include "haylen/3d/physics/World.hpp"
#include "haylen/3d/physics/WorldSettings.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/math/Math.hpp"

namespace haylen::physics3d {

class PhysicsWorld3DTest : public ::testing::Test {
  protected:
    // Returns the message of the `std::invalid_argument` that the call throws, or an empty text when it throws none.
    template <typename Call> static std::string refusal(Call call) {
        try {
            call();
        } catch (const std::invalid_argument& error) {
            return error.what();
        }

        return {};
    }

    jolt::Backend backend;
};

TEST_F(PhysicsWorld3DTest, PullsDownWithStandardGravityInMeters) {
    const World world({}, backend);
    EXPECT_EQ(world.getGravity().x, 0.0F);
    EXPECT_EQ(world.getGravity().y, -math::Math::kStandardGravity);
    EXPECT_EQ(world.getGravity().z, 0.0F);
    EXPECT_EQ(world.getBackendName(), "jolt");
}

TEST_F(PhysicsWorld3DTest, RefusesSettingsOutsideTheirRange) {
    EXPECT_EQ(refusal([this] { World(WorldSettings{.velocitySteps = 1}, backend); }), "The value of \"velocitySteps\" must be from 2 to 64.");
    EXPECT_EQ(refusal([this] { World(WorldSettings{.maxBodies = 0}, backend); }), "The value of \"maxBodies\" must be from 1 to 8388607.");
    EXPECT_EQ(refusal([this] { World(WorldSettings{.gravity = {0.0F, std::numeric_limits<float>::infinity(), 0.0F}}, backend); }), "The value of \"gravity\" must be a vector of three finite numbers.");
    EXPECT_EQ(refusal([this] { World(WorldSettings{.threads = 2}, backend); }), "A 3D physics world that steps on more than one thread needs the job system of the engine.");
}

TEST_F(PhysicsWorld3DTest, ChangesItsSettingsAndKeepsThemWhenAValueIsRefused) {
    World world({}, backend);
    world.setGravity({0.0F, -1.62F, 0.0F});
    world.setVelocitySteps(12);
    world.setSleepEnabled(false);
    EXPECT_EQ(world.getGravity().y, -1.62F);
    EXPECT_EQ(world.getVelocitySteps(), 12);
    EXPECT_FALSE(world.isSleepEnabled());

    EXPECT_EQ(refusal([&world] { world.setPositionSteps(65); }), "The value of \"positionSteps\" must be from 0 to 64.");
    EXPECT_EQ(world.getPositionSteps(), 2);
}

TEST_F(PhysicsWorld3DTest, StepsByATimeOfZeroSecondsOrMore) {
    World world({}, backend);
    world.step(1.0F / 60.0F);
    world.step(0.0F);
    EXPECT_EQ(world.getLastStep(), 0.0F);
    EXPECT_EQ(world.getStats().bodies, 0);
    EXPECT_EQ(refusal([&world] { world.step(-1.0F); }), "A step of a 3D physics world takes a time of zero seconds or more.");
    EXPECT_EQ(refusal([&world] { world.step(std::nanf("")); }), "A step of a 3D physics world takes a time of zero seconds or more.");
}

TEST_F(PhysicsWorld3DTest, EndsEveryLaterCallOnceDestroyed) {
    World world({}, backend);
    world.destroy();
    world.destroy();
    EXPECT_TRUE(world.isDestroyed());
    EXPECT_THROW(world.step(1.0F / 60.0F), std::logic_error);
    EXPECT_THROW(world.setGravity({}), std::logic_error);
    EXPECT_THROW(static_cast<void>(world.getStats()), std::logic_error);
    EXPECT_THROW(static_cast<void>(world.computeStateHash()), std::logic_error);
}

TEST_F(PhysicsWorld3DTest, HashesTheSameStateOfTwoWorldsThatStepAlike) {
    World first({}, backend);
    World second({}, backend);
    for (int step = 0; step < 10; ++step) {
        first.step(1.0F / 60.0F);
        second.step(1.0F / 60.0F);
    }

    EXPECT_EQ(first.computeStateHash(), second.computeStateHash());
}

TEST_F(PhysicsWorld3DTest, ReadsThePhysics3DSectionOfHaylenJson) {
    WorldSettings settings;
    settings.readSection(core::Json::parse(R"({"gravity": [0, -1.62, 0], "velocitySteps": 8, "sleepEnabled": false, "maxBodies": 2048, "temporaryMemory": 4})"));
    EXPECT_EQ(settings.gravity.y, -1.62F);
    EXPECT_EQ(settings.velocitySteps, 8);
    EXPECT_FALSE(settings.sleepEnabled);
    EXPECT_EQ(settings.maxBodies, 2048);
    EXPECT_EQ(settings.temporaryMemory, 4);
}

TEST_F(PhysicsWorld3DTest, NamesTheKeyOfHaylenJsonThatItRefuses) {
    WorldSettings settings;
    EXPECT_EQ(refusal([&settings] { settings.readSection(core::Json::parse(R"({"velocity": 8})")); }), "Unknown key \"velocity\" in the \"physics3d\" section of \"haylen.json\".");
    EXPECT_EQ(refusal([&settings] { settings.readSection(core::Json::parse(R"({"velocitySteps": 8.5})")); }), "The value of \"physics3d.velocitySteps\" in \"haylen.json\" has the wrong type.");
    EXPECT_EQ(refusal([&settings] { settings.readSection(core::Json::parse(R"({"sleepEnabled": 1})")); }), "The value of \"physics3d.sleepEnabled\" in \"haylen.json\" has the wrong type.");
    EXPECT_EQ(refusal([&settings] { settings.readSection(core::Json::parse(R"({"gravity": [0, -9.8]})")); }), "The value of \"physics3d.gravity\" in \"haylen.json\" must be a list of three numbers, such as [0, -1.62, 0].");
    EXPECT_EQ(refusal([&settings] { settings.readSection(core::Json::parse(R"({"threads": 0})")); }), "The value of \"physics3d.threads\" in \"haylen.json\" must be from 1 to 64.");
    EXPECT_EQ(refusal([&settings] { settings.readSection(core::Json::parse("[]")); }), "The \"physics3d\" section of \"haylen.json\" must be an object of settings, such as {\"velocitySteps\": 10}.");
}

} // namespace haylen::physics3d
