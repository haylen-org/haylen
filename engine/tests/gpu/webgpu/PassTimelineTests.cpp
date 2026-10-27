#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <optional>

#include "gpu/webgpu/PassTimeline.hpp"

namespace haylen::gpu::webgpu {

// The passes of a frame whose timestamps at the start of each pass are taken while the GPU still works on the passes ahead of it, so each pass seems to last from then on. Adding their spans up counted the same work once per pass.
TEST(WebGpuPassTimelineTest, CountsTheWorkOfPassesThatStartBeforeThePassesAheadOfThemEnd) {
    PassTimeline timeline;
    const std::array<std::uint64_t, 2> first{1'000'000, 3'000'000};
    const std::optional<double> alone = timeline.addFrame(1, first);
    ASSERT_TRUE(alone.has_value());
    EXPECT_DOUBLE_EQ(*alone, 0.002);

    const std::array<std::uint64_t, 6> second{2'000'000, 5'000'000, 2'000'100, 7'000'000, 2'000'200, 8'000'000};
    const std::optional<double> seconds = timeline.addFrame(2, second);
    ASSERT_TRUE(seconds.has_value());
    EXPECT_DOUBLE_EQ(*seconds, 0.005);
}

// The idle time between passes and between frames is no work of the GPU.
TEST(WebGpuPassTimelineTest, LeavesOutTheTimeBetweenPasses) {
    PassTimeline timeline;
    const std::array<std::uint64_t, 2> first{100, 200};
    static_cast<void>(timeline.addFrame(7, first));
    const std::array<std::uint64_t, 4> second{1'000'000, 1'250'000, 2'000'000, 2'500'000};
    const std::optional<double> seconds = timeline.addFrame(8, second);
    ASSERT_TRUE(seconds.has_value());
    EXPECT_DOUBLE_EQ(*seconds, 0.00075);
}

// Only the frame submitted right before a frame bounds its first pass, so a frame after one that went unmeasured counts its first pass whole.
TEST(WebGpuPassTimelineTest, BoundsTheFirstPassOnlyByTheFrameSubmittedBefore) {
    PassTimeline timeline;
    const std::array<std::uint64_t, 2> first{1'000, 2'000};
    static_cast<void>(timeline.addFrame(1, first));
    const std::array<std::uint64_t, 2> third{1'500, 4'000};
    const std::optional<double> whole = timeline.addFrame(3, third);
    ASSERT_TRUE(whole.has_value());
    EXPECT_DOUBLE_EQ(*whole, 0.0000025);
    const std::array<std::uint64_t, 2> fourth{3'000, 5'000};
    const std::optional<double> bounded = timeline.addFrame(4, fourth);
    ASSERT_TRUE(bounded.has_value());
    EXPECT_DOUBLE_EQ(*bounded, 0.000001);
}

// A timestamp that the queries never wrote reads zero, and one that runs backwards tells nothing either, so such a frame gives no time and bounds no frame after it.
TEST(WebGpuPassTimelineTest, GivesNoTimeForTimestampsThatCannotBeTrusted) {
    PassTimeline timeline;
    const std::array<std::uint64_t, 2> frame{1'000, 2'000};
    static_cast<void>(timeline.addFrame(1, frame));
    const std::array<std::uint64_t, 4> unwritten{0, 3'000'000'000, 2'500, 3'000};
    EXPECT_FALSE(timeline.addFrame(2, unwritten).has_value());
    const std::array<std::uint64_t, 2> after{1'500, 5'000};
    const std::optional<double> whole = timeline.addFrame(3, after);
    ASSERT_TRUE(whole.has_value());
    EXPECT_DOUBLE_EQ(*whole, 0.0000035);
    const std::array<std::uint64_t, 2> backwards{9'000, 8'000};
    EXPECT_FALSE(timeline.addFrame(4, backwards).has_value());
}

} // namespace haylen::gpu::webgpu
