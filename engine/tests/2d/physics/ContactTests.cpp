#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "haylen/2d/physics/Body.hpp"
#include "haylen/2d/physics/ContactEvent.hpp"
#include "haylen/2d/physics/World.hpp"

namespace haylen::physics2d {

namespace {

// A crate flung at the floor at 10 meters per second, in a world without gravity, so its impact is the only thing that stops it.
class ContactTest : public ::testing::Test {
  protected:
    static constexpr float kStep = 1.0F / 60.0F;
    static constexpr float kSpeed = 640.0F;

    ContactTest() {
        Body floor = world.createBody({.type = Body::Type::Static, .position = {0.0F, 200.0F}});
        floor.addBox({400.0F, 20.0F});
        crate.addBox({32.0F, 32.0F});
    }

    // Steps until a contact begins, and returns the step it began in.
    std::vector<ContactEvent> stepUntilContact() {
        for (int step = 0; step < 120; ++step) {
            world.step(kStep);
            if (!world.getContactBegins().empty()) {
                return world.getContactBegins();
            }
        }
        return {};
    }

    World world{{.gravity = {}, .pixelsPerMeter = 64.0F}};
    Body crate = world.createBody({.position = {0.0F, 0.0F}, .velocity = {0.0F, kSpeed}});
};

} // namespace

// A contact that begins reports the impact the step solved: the approach speed of its points before the solve, the impulse that stopped the crate and the force it stands for, and how the bodies move after it.
TEST_F(ContactTest, ReportsTheImpactOfAContactThatBegins) {
    const std::vector<ContactEvent> begins = stepUntilContact();
    ASSERT_EQ(begins.size(), 1U);
    const ContactManifold& impact = begins[0].manifold;
    ASSERT_EQ(impact.pointCount, 2U);
    EXPECT_NEAR(impact.approachSpeed, kSpeed, kSpeed * 0.05F);
    EXPECT_NEAR(std::fabs(impact.normal.y), 1.0F, 1e-4F);
    EXPECT_FLOAT_EQ(impact.force, (impact.points[0].normalImpulse + impact.points[1].normalImpulse) / (kStep / static_cast<float>(world.getSubSteps())));
    EXPECT_NEAR(impact.points[0].point.y, 190.0F, 2.0F);
    EXPECT_LT(impact.points[0].separation, 1.0F);

    // The step stopped the crate, so it no longer moves against the floor.
    EXPECT_LT(impact.relativeVelocity.getLength(), kSpeed * 0.05F);
}

// A hit reports the approach speed of Box2D and the points and impulses of the contact it hit with.
TEST_F(ContactTest, ReportsTheContactOfAHit) {
    std::vector<ContactEvent> hits;
    for (int step = 0; step < 120 && hits.empty(); ++step) {
        world.step(kStep);
        hits = world.getContactHits();
    }
    ASSERT_EQ(hits.size(), 1U);
    EXPECT_NEAR(hits[0].manifold.approachSpeed, kSpeed, kSpeed * 0.05F);
    EXPECT_EQ(hits[0].manifold.pointCount, 2U);
}

// A crate at rest on the floor pushes it with its weight, so the force of its contact is its mass times the gravity, and it closes in on nothing.
TEST(ContactRestTest, ReportsTheWeightOfABodyAtRest) {
    World world({.gravity = {0.0F, 980.0F}, .pixelsPerMeter = 64.0F});
    Body floor = world.createBody({.type = Body::Type::Static, .position = {0.0F, 100.0F}});
    floor.addBox({400.0F, 20.0F});
    Body crate = world.createBody({.position = {0.0F, 70.0F}});
    crate.addBox({40.0F, 40.0F});
    for (int step = 0; step < 120; ++step) {
        world.step(1.0F / 60.0F);
    }
    const std::vector<Body::Contact> contacts = crate.getContacts();
    ASSERT_EQ(contacts.size(), 1U);
    const ContactManifold& resting = contacts[0].manifold;
    EXPECT_EQ(resting.pointCount, 2U);
    EXPECT_NEAR(resting.force, crate.getMass() * 980.0F, crate.getMass() * 980.0F * 0.02F);
    EXPECT_LT(resting.approachSpeed, 20.0F);
    EXPECT_NEAR(resting.normal.y, 1.0F, 1e-4F);
    EXPECT_NEAR(resting.points[0].point.y, 90.0F, 1.0F);
}

} // namespace haylen::physics2d
