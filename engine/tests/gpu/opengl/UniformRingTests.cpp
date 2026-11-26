#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <span>
#include <vector>

#include "gpu/opengl/Context.hpp"
#include "gpu/opengl/GlCallCounter.hpp"
#include "gpu/opengl/OpenGlTest.hpp"
#include "gpu/opengl/UniformRing.hpp"

namespace haylen::gpu::opengl {

class UniformRingTest : public OpenGlTest {
  protected:
    static constexpr std::size_t kAlignment = 256;

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

// A slot whose bytes come again in the frame takes the range they went to, and new bytes go to the next aligned range.
TEST_P(UniformRingTest, WritesABlockAgainOnlyWhenItsBytesChange) {
    UniformRing ring(opened.gl, opened.state, 3, kAlignment);
    ring.beginFrame(0);
    std::array<float, 4> values{1.0F, 0.0F, 0.0F, 1.0F};

    const StateCache::UniformRange first = ring.write(0, std::as_bytes(std::span(values)));
    const StateCache::UniformRange again = ring.write(0, std::as_bytes(std::span(values)));
    EXPECT_EQ(again, first);

    values[1] = 1.0F;
    const StateCache::UniformRange changed = ring.write(0, std::as_bytes(std::span(values)));
    EXPECT_EQ(changed.buffer, first.buffer);
    EXPECT_EQ(changed.offset, first.offset + static_cast<Gl::IntPtr>(kAlignment));

    ring.beginFrame(3);
    EXPECT_EQ(ring.write(0, std::as_bytes(std::span(values))).offset, 0);
}

// A frame that fills a chunk moves on to another one, so the ranges it bound before keep their buffer, and a trim lets go only of the chunks that the last frame of each region did not write.
TEST_P(UniformRingTest, KeepsTheChunksThatItsFrameBound) {
    UniformRing ring(opened.gl, opened.state, 3, kAlignment);
    ring.beginFrame(0);
    const std::vector<std::byte> block(UniformRing::kChunkSize / 2 + kAlignment);

    const StateCache::UniformRange first = ring.write(0, block);
    const StateCache::UniformRange second = ring.write(1, block);
    EXPECT_NE(second.buffer, first.buffer);
    EXPECT_EQ(second.offset, 0);

    ring.trim();
    EXPECT_EQ(ring.getSpareBytes(), 0U);
    ring.beginFrame(3);
    static_cast<void>(ring.write(0, block));
    EXPECT_EQ(ring.getSpareBytes(), UniformRing::kChunkSize);
    ring.trim();
    EXPECT_EQ(ring.getSpareBytes(), 0U);
}

// The state cache binds a range only when the binding point holds another one, so a block that repeats in the frame binds nothing.
TEST_P(UniformRingTest, BindsARepeatedBlockOnce) {
    UniformRing ring(opened.gl, opened.state, 3, kAlignment);
    ring.beginFrame(0);
    const std::array<float, 4> values{0.0F, 1.0F, 0.0F, 1.0F};
    GlCallCounter counter(opened.gl);
    counter.watch<&Gl::bindBufferRange>();

    opened.state.bindUniformRange(0, ring.write(0, std::as_bytes(std::span(values))));
    opened.state.bindUniformRange(0, ring.write(0, std::as_bytes(std::span(values))));
    EXPECT_EQ(counter.getCalls(), 1);
}

INSTANTIATE_TEST_SUITE_P(Flavours, UniformRingTest, ::testing::Values(Device::Api::OpenGl, Device::Api::OpenGlEs), OpenGlTest::getFlavourName);

} // namespace haylen::gpu::opengl
