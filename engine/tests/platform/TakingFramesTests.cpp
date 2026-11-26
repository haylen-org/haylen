#include <gtest/gtest.h>

#include <cstdint>

#include "gpu/NullDevice.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/TimerScheduler.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::platform {

// While the window system takes no frame of the window, such as a Wayland compositor whose outputs sleep, the frames run the app and draw nothing, and the app draws again as soon as the window system takes frames.
class TakingFramesTest : public ::testing::Test {
  protected:
    [[nodiscard]] std::uint64_t committed() {
        return app.host().getGpu().getCommittedFrames();
    }

    test::EngineFixture app{{{"haylen.json", R"({"name": "Test App", "identifier": "com.haylen.tests", "window": {"redraw": "onChange"}})"}}};
};

TEST_F(TakingFramesTest, RunsTimersWithoutDrawingWhileTheWindowTakesNoFrame) {
    app.frames(10);
    int ticks = 0;
    (void)app.engine().getTimers().every(0.1F, [&ticks] { ++ticks; });

    app.host().setTakingFrames(false);
    const std::uint64_t before = committed();
    app.frames(60);
    EXPECT_GE(ticks, 9);
    EXPECT_EQ(committed(), before);
    EXPECT_FALSE(app.engine().isDrawing());
}

TEST_F(TakingFramesTest, DrawsAtOnceWhenTheWindowTakesFramesAgain) {
    app.frames(10);
    app.host().setTakingFrames(false);
    app.engine().requestRedraw();
    app.frames(30);
    const std::uint64_t before = committed();

    app.host().setTakingFrames(true);
    app.frames(1);
    EXPECT_EQ(committed(), before + 1);
    EXPECT_TRUE(app.engine().isDrawing());
}

} // namespace haylen::platform
