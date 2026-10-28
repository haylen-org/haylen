#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <optional>

#include "gpu/opengl/Context.hpp"
#include "gpu/opengl/GlCallCounter.hpp"
#include "gpu/opengl/GpuTimer.hpp"
#include "gpu/opengl/OpenGlTest.hpp"

namespace haylen::gpu::opengl {

class GpuTimerTest : public OpenGlTest {
  protected:
    void SetUp() override {
        OpenGlTest::SetUp();
        if (IsSkipped()) {
            return;
        }
        opened.gl.load(*context, GetParam());
        if (!opened.gl.getFeatures().timerQuery) {
            GTEST_SKIP() << "The context measures no time on the GPU.";
        }
    }

    void drawFrame(GpuTimer& timer) {
        const std::array<Gl::Float, 4> black{0.0F, 0.0F, 0.0F, 1.0F};
        timer.beginFrame();
        for (int pass = 0; pass < 2; ++pass) {
            timer.beginPass();
            opened.gl.clearBufferfv(Gl::kColor, 0, black.data());
            timer.endPass();
            opened.gl.flush();
        }
        timer.endFrame();
    }

    Context opened;
};

// Each pass ends its query before the flush that may end the pass, since the OpenGL ES driver r19p0 of the Mali-G72 never finishes a query that spans one.
TEST_P(GpuTimerTest, EndsTheQueryOfEachPassWithThePass) {
    GpuTimer timer(opened.gl);
    GlCallCounter counter(opened.gl);
    counter.watch<&Gl::beginQuery>();
    counter.watch<&Gl::endQuery>();

    timer.beginFrame();
    EXPECT_EQ(counter.getCalls(), 0);
    timer.beginPass();
    EXPECT_EQ(counter.getCalls(), 1);
    timer.endPass();
    EXPECT_EQ(counter.getCalls(), 2);
    timer.beginPass();
    timer.endPass();
    EXPECT_EQ(counter.getCalls(), 4);
    timer.endFrame();
    EXPECT_EQ(counter.getCalls(), 4);
}

// The time of a frame is the sum of its passes, which a later frame reads once every query of the frame is there.
TEST_P(GpuTimerTest, MeasuresTheFramesWhosePassesFinished) {
    GpuTimer timer(opened.gl);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!timer.getSeconds() && std::chrono::steady_clock::now() < deadline) {
        drawFrame(timer);
    }
    ASSERT_TRUE(timer.getSeconds().has_value());
    EXPECT_GE(*timer.getSeconds(), 0.0);
}

// A driver that refuses the disjoint state, as the OpenGL ES driver 25.10 of AMD does, is never asked for it and still has its frames measured.
TEST_P(GpuTimerTest, MeasuresWithoutAskingForTheDisjointStateWhereTheDriverRefusesIt) {
    GpuTimer timer(opened.gl);
    timer.setReadsDisjoint(false);
    GlCallCounter counter(opened.gl);
    counter.watch<&Gl::getIntegerv>();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!timer.getSeconds() && std::chrono::steady_clock::now() < deadline) {
        drawFrame(timer);
    }
    ASSERT_TRUE(timer.getSeconds().has_value());
    EXPECT_EQ(counter.getCalls(), 0);
    EXPECT_EQ(opened.gl.getError(), Gl::kNoError);
}

INSTANTIATE_TEST_SUITE_P(Flavours, GpuTimerTest, ::testing::Values(Device::Api::OpenGl, Device::Api::OpenGlEs), OpenGlTest::getFlavourName);

} // namespace haylen::gpu::opengl
