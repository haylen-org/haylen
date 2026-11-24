#include <gtest/gtest.h>

#include <span>
#include <stdexcept>
#include <vector>

#include "haylen/debug/ProfileScope.hpp"
#include "haylen/debug/Profiler.hpp"
#include "support/AllocationTracker.hpp"

namespace haylen::debug {

TEST(ProfilerTest, AddsUpScopesPerFrame) {
    double now = 0.0;
    Profiler profiler(4, [&now] { return now; });

    profiler.beginFrame(0.0);
    now = 0.001;
    profiler.beginScope("update");
    now = 0.003;
    profiler.beginScope("physics");
    now = 0.004;
    profiler.endScope();
    {
        const ProfileScope scope(profiler, "physics");
        now = 0.006;
    }

    now = 0.007;
    profiler.endScope();
    profiler.beginScope("render");
    now = 0.010;
    profiler.endScope();
    now = 0.016;
    profiler.endFrame();

    const std::span<const ProfileSample> samples = profiler.getLastFrame();
    ASSERT_EQ(samples.size(), 3U);
    EXPECT_EQ(samples[0].name, "update");
    EXPECT_NEAR(samples[0].milliseconds, 6.0, 1e-9);
    EXPECT_EQ(samples[1].name, "physics");
    EXPECT_NEAR(samples[1].milliseconds, 3.0, 1e-9);
    EXPECT_EQ(samples[1].calls, 2U);
    EXPECT_EQ(samples[1].depth, 1);
    EXPECT_EQ(samples[1].parent, 0);
    EXPECT_EQ(samples[2].name, "render");
    EXPECT_EQ(samples[2].depth, 0);
    EXPECT_NEAR(profiler.getLastFrameMilliseconds(), 16.0, 1e-9);
}

// Frames of the same scopes reuse the samples of the frames before them, so a scope whose name is longer than the text a string keeps in place allocates nothing, as on the web, where that text is shortest.
TEST(ProfilerTest, MeasuresFramesOfTheSameScopesWithoutAllocating) {
    Profiler profiler;
    // clang-format off
    const auto frame = [&profiler] {
        profiler.beginFrame(0.016);
        const ProfileScope outer(profiler, "a scope whose name outgrows the text of a short string");
        const ProfileScope inner(profiler, "another scope inside it with a long name");
    };
    // clang-format on
    frame();
    profiler.endFrame();
    frame();
    profiler.endFrame();

    const test::AllocationTracker tracker;
    for (int index = 0; index < 3; ++index) {
        frame();
        profiler.endFrame();
    }

    EXPECT_EQ(tracker.getTotal(), 0U);
    ASSERT_EQ(profiler.getLastFrame().size(), 2U);
    EXPECT_EQ(profiler.getLastFrame()[1].name, "another scope inside it with a long name");
}

TEST(ProfilerTest, ScopesCloseWhatTheyContain) {
    Profiler profiler;
    profiler.beginFrame(0.0);
    {
        const ProfileScope outer(profiler, "outer");
        profiler.beginScope("left open");
    }

    EXPECT_EQ(profiler.getDepth(), 0U);

    // A scope that something else already closed ends quietly.
    {
        const ProfileScope closed(profiler, "closed early");
        profiler.endScope();
    }

    EXPECT_EQ(profiler.getDepth(), 0U);
    profiler.endFrame();
    EXPECT_EQ(profiler.getLastFrame().size(), 3U);
}

TEST(ProfilerTest, KeepsARingOfFrameTimes) {
    double now = 0.0;
    Profiler profiler(4, [&now] { return now; });
    EXPECT_EQ(profiler.getAverageFrameMilliseconds(), 0.0);
    std::vector<float> history;
    profiler.readFrameHistory(history);
    EXPECT_TRUE(history.empty());

    for (int frame = 1; frame <= 6; ++frame) {
        profiler.beginFrame(0.0);
        now += 0.001 * frame;
        profiler.endFrame();
    }

    profiler.readFrameHistory(history);
    ASSERT_EQ(history.size(), 4U);
    EXPECT_NEAR(history.front(), 3.0F, 1e-4F);
    EXPECT_NEAR(history.back(), 6.0F, 1e-4F);
    EXPECT_NEAR(profiler.getAverageFrameMilliseconds(), 4.5, 1e-4);

    // A scope an error left open closes with the frame.
    profiler.beginFrame(0.0);
    profiler.beginScope("broken");
    now += 0.002;
    profiler.endFrame();
    ASSERT_FALSE(profiler.getLastFrame().empty());
    EXPECT_EQ(profiler.getLastFrame().front().name, "broken");
    EXPECT_NEAR(profiler.getLastFrame().front().milliseconds, 2.0, 1e-6);

    EXPECT_THROW(profiler.endScope(), std::logic_error);
    EXPECT_THROW(profiler.endFrame(), std::logic_error);
    EXPECT_THROW(Profiler(0), std::invalid_argument);
}

// The frame rate counts the intervals at which the display showed the frames, which hold the wait for the display and the frames it missed that the work of a frame leaves out, and reading it allocates nothing.
TEST(ProfilerTest, MeasuresTheFrameRateFromTheFramesTheDisplayShowed) {
    double now = 0.0;
    Profiler profiler(8, [&now] { return now; });
    profiler.beginFrame(0.0);
    EXPECT_EQ(profiler.getFramesPerSecond(), 0.0);

    for (int frame = 0; frame < 20; ++frame) {
        now += 0.002;
        profiler.endFrame();
        profiler.beginFrame(frame % 4 == 0 ? 2.0 / 60.0 : 1.0 / 60.0);
    }

    now += 0.002;
    profiler.endFrame();

    const test::AllocationTracker tracker;
    const double rate = profiler.getFramesPerSecond();
    const double work = profiler.getAverageFrameMilliseconds();
    EXPECT_EQ(tracker.getTotal(), 0U);
    EXPECT_NEAR(rate, 8.0 / (10.0 / 60.0), 0.01);
    EXPECT_NEAR(work, 2.0, 1e-4);
}

// A script closes only the scopes it opened, so closing one too many leaves the scope of the engine around it open.
TEST(ProfilerTest, KeepsTheScopesOfTheEngineFromScripts) {
    Profiler profiler;
    profiler.beginFrame(0.0);
    profiler.beginScope("update");
    profiler.beginScriptScope("enemies");
    profiler.endScriptScope();
    EXPECT_THROW(profiler.endScriptScope(), std::logic_error);
    EXPECT_EQ(profiler.getDepth(), 1U);
    profiler.endScope();
    profiler.endFrame();
}

} // namespace haylen::debug
