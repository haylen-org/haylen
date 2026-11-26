#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/3d/animation/ClipPlayer3D.hpp"
#include "support/AllocationTracker.hpp"

namespace haylen::animation3d {

// Plays a clip of one second whose last pose is its first, with events at its start, its middle and its end, and records what the player reports.
class ClipPlayer3DTest : public ::testing::Test {
  protected:
    void SetUp() override {
        std::vector<Track3D> tracks;
        tracks.push_back(Track3D({.target = "Door", .property = "angle", .times = {0.0F, 0.5F, 1.0F}, .values = {0.0F, 2.0F, 0.0F}}));
        clip = std::make_shared<const Clip3D>(Clip3D({.name = "swing", .tracks = std::move(tracks), .events = {{.time = 0.0F, .name = "start"}, {.time = 0.5F, .name = "middle"}, {.time = 1.0F, .name = "end"}}}));
    }

    // Connects a player to the record of its events and its end.
    void listen(ClipPlayer3D& player) {
        player.eventFired.connect([this](const Clip3D::Event& event) { reported.push_back(event.name); });
        player.completed.connect([this] { reported.emplace_back("complete"); });
    }

    // Updates in steps and fails when the time moves by more than the step allows, other than by wrapping from one end to the other.
    void expectSmooth(ClipPlayer3D& player, float step, int count) {
        for (int update = 0; update < count; ++update) {
            const float before = player.getTime();
            const float angleBefore = player.getValues()[0];
            player.update(step);
            const float moved = std::abs(player.getTime() - before);
            EXPECT_LE(std::min(moved, 1.0F - moved), std::abs(step * player.getSpeed()) + 1e-5F) << update;
            EXPECT_LE(std::abs(player.getValues()[0] - angleBefore), std::abs(step * player.getSpeed()) * 4.0F + 1e-4F) << update;
        }
    }

    std::shared_ptr<const Clip3D> clip;
    std::vector<std::string> reported;
};

// A looping player wraps from its end to its start without a jump and reports every event of each pass once, in order.
TEST_F(ClipPlayer3DTest, LoopsForwardWithoutJumps) {
    ClipPlayer3D player(clip);
    listen(player);
    expectSmooth(player, 0.05F, 51);
    EXPECT_NEAR(player.getTime(), 0.55F, 1e-4F);
    EXPECT_EQ(reported, (std::vector<std::string>{"start", "middle", "end", "start", "middle", "end", "start", "middle"}));
}

// A negative speed plays backward, wraps from the start to the end and reports the events in the order it meets them.
TEST_F(ClipPlayer3DTest, PlaysBackwardWithANegativeSpeed) {
    ClipPlayer3D player(clip, {.speed = -1.0F});
    listen(player);
    EXPECT_EQ(player.getTime(), 1.0F);
    expectSmooth(player, 0.05F, 31);
    EXPECT_NEAR(player.getTime(), 0.45F, 1e-4F);
    EXPECT_EQ(reported, (std::vector<std::string>{"end", "middle", "start", "end", "middle"}));
}

// A paused player keeps its time and reports nothing, and goes on where it stopped.
TEST_F(ClipPlayer3DTest, PausesWhereItStands) {
    ClipPlayer3D player(clip);
    listen(player);
    player.update(0.25F);
    player.setPaused(true);
    player.update(5.0F);
    EXPECT_EQ(player.getTime(), 0.25F);
    EXPECT_EQ(player.getValues()[0], 1.0F);
    player.setPaused(false);
    player.update(0.25F);
    EXPECT_EQ(reported, (std::vector<std::string>{"start", "middle"}));
}

// A player that plays once stops at its end, reports the end once, and leaves it when its speed turns back.
TEST_F(ClipPlayer3DTest, StopsAtTheEndOnce) {
    ClipPlayer3D player(clip, {.loop = ClipPlayer3D::Loop::Once});
    listen(player);
    player.update(0.7F);
    player.update(0.7F);
    player.update(0.7F);
    EXPECT_EQ(player.getTime(), 1.0F);
    EXPECT_TRUE(player.isFinished());
    EXPECT_EQ(reported, (std::vector<std::string>{"start", "middle", "end", "complete"}));

    player.setSpeed(-1.0F);
    player.update(0.25F);
    EXPECT_FALSE(player.isFinished());
    EXPECT_EQ(player.getTime(), 0.75F);
}

// A player that turns at the ends plays forward and back, reports the event at each end once per turn and the event between on both ways.
TEST_F(ClipPlayer3DTest, TurnsAtTheEnds) {
    ClipPlayer3D player(clip, {.loop = ClipPlayer3D::Loop::PingPong});
    listen(player);
    expectSmooth(player, 0.1F, 26);
    EXPECT_NEAR(player.getTime(), 0.6F, 1e-4F);
    EXPECT_EQ(reported, (std::vector<std::string>{"start", "middle", "end", "middle", "start", "middle"}));
}

// An update far longer than the clip reports the events of its last passes only, and lands where the remainder of its time puts it.
TEST_F(ClipPlayer3DTest, ReportsTheLastPassesOfALongUpdate) {
    ClipPlayer3D player(clip);
    listen(player);
    player.update(1000.25F);
    EXPECT_NEAR(player.getTime(), 0.25F, 1e-3F);
    EXPECT_EQ(reported, (std::vector<std::string>{"start", "middle", "end", "start", "middle", "end", "start"}));
}

// Setting the time moves without passing the events between, and the next update reports the events at that time.
TEST_F(ClipPlayer3DTest, SetsTheTimeWithoutPassingEvents) {
    ClipPlayer3D player(clip);
    listen(player);
    player.setTime(0.5F);
    EXPECT_EQ(player.getValues()[0], 2.0F);
    EXPECT_TRUE(reported.empty());
    player.update(0.1F);
    EXPECT_EQ(reported, (std::vector<std::string>{"middle"}));
    EXPECT_THROW(player.setTime(1.5F), std::invalid_argument);
    EXPECT_THROW(player.update(-1.0F), std::invalid_argument);
}

// Updates that pass no event allocate nothing, and a listener cannot update the player it listens to.
TEST_F(ClipPlayer3DTest, UpdatesWithoutAllocating) {
    ClipPlayer3D player(clip);
    player.update(0.05F);
    {
        const test::AllocationTracker tracker;
        for (int frame = 0; frame < 8; ++frame) {
            player.update(0.05F);
        }

        EXPECT_EQ(tracker.getTotal(), 0U);
    }

    player.eventFired.connect([&player](const Clip3D::Event&) { player.update(0.1F); });
    EXPECT_THROW(player.update(0.2F), std::logic_error);
}

} // namespace haylen::animation3d
