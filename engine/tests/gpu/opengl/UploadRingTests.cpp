#include <gtest/gtest.h>

#include <cstring>

#include "gpu/opengl/Context.hpp"
#include "gpu/opengl/OpenGlTest.hpp"
#include "gpu/opengl/UploadRing.hpp"

namespace haylen::gpu::opengl {

class UploadRingTest : public OpenGlTest {
  protected:
    void SetUp() override {
        OpenGlTest::SetUp();
        if (IsSkipped()) {
            return;
        }
        opened.gl.load(*context, GetParam());
        opened.state.reset();
    }

    Context opened;
};

TEST_P(UploadRingTest, WritesEachFrameIntoItsOwnRegion) {
    UploadRing ring(opened.gl, opened.state, 3);
    ring.beginFrame(4);
    const UploadRing::Slice slice = ring.map(100);
    std::memset(slice.data, 7, 100);
    ring.unmap();
    EXPECT_EQ(slice.offset, UploadRing::kInitialRegionSize);

    const UploadRing::Slice next = ring.map(10);
    ring.unmap();
    EXPECT_EQ(next.offset, UploadRing::kInitialRegionSize + 112);
}

TEST_P(UploadRingTest, GrowsWhenAFrameWritesMoreThanItsRegion) {
    UploadRing ring(opened.gl, opened.state, 3);
    ring.beginFrame(0);
    static_cast<void>(ring.map(UploadRing::kInitialRegionSize / 2));
    ring.unmap();
    const UploadRing::Slice slice = ring.map(UploadRing::kInitialRegionSize);
    std::memset(slice.data, 1, UploadRing::kInitialRegionSize);
    ring.unmap();
    EXPECT_EQ(ring.getSize(), UploadRing::kInitialRegionSize * 2 * 3);
    EXPECT_EQ(slice.offset, 0U);
}

INSTANTIATE_TEST_SUITE_P(Flavours, UploadRingTest, ::testing::Values(Device::Api::OpenGl, Device::Api::OpenGlEs), OpenGlTest::getFlavourName);

} // namespace haylen::gpu::opengl
