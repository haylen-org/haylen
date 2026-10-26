#include <gtest/gtest.h>

#include "gpu/opengl/Context.hpp"
#include "gpu/opengl/GlCallCounter.hpp"
#include "gpu/opengl/OpenGlTest.hpp"

namespace haylen::gpu::opengl {

class StateCacheTest : public OpenGlTest {
  protected:
    void SetUp() override {
        OpenGlTest::SetUp();
        if (IsSkipped()) {
            return;
        }
        opened.gl.load(*context, GetParam());
        opened.state.reset();
    }

    // Watches every entry point that sets the fixed function state of a pipeline.
    static void watchRaster(GlCallCounter& counter) {
        counter.watch<&Gl::enable>();
        counter.watch<&Gl::disable>();
        counter.watch<&Gl::enablei>();
        counter.watch<&Gl::disablei>();
        counter.watch<&Gl::blendFuncSeparate>();
        counter.watch<&Gl::blendEquationSeparate>();
        counter.watch<&Gl::blendFuncSeparatei>();
        counter.watch<&Gl::blendEquationSeparatei>();
        counter.watch<&Gl::colorMask>();
        counter.watch<&Gl::colorMaski>();
        counter.watch<&Gl::depthFunc>();
        counter.watch<&Gl::depthMask>();
        counter.watch<&Gl::stencilFuncSeparate>();
        counter.watch<&Gl::stencilOpSeparate>();
        counter.watch<&Gl::stencilMaskSeparate>();
        counter.watch<&Gl::cullFace>();
        counter.watch<&Gl::frontFace>();
        counter.watch<&Gl::polygonOffset>();
    }

    [[nodiscard]] static StateCache::Raster makeBlendedRaster() {
        StateCache::Raster raster;
        raster.blends.fill({.enabled = true, .sourceColor = Gl::kSourceAlpha, .destinationColor = Gl::kOneMinusSourceAlpha});
        raster.masks.fill(7);
        raster.depthTest = true;
        raster.depthCompare = Gl::kLessEqual;
        raster.depthWrite = true;
        raster.stencilTest = true;
        raster.stencilFront = {.compare = Gl::kEqual, .pass = Gl::kReplace};
        raster.stencilBack = raster.stencilFront;
        raster.cull = true;
        return raster;
    }

    Context opened;
};

TEST_P(StateCacheTest, SendsNothingWhenARasterRepeats) {
    const StateCache::Raster raster = makeBlendedRaster();
    GlCallCounter counter(opened.gl);
    watchRaster(counter);

    opened.state.applyRaster(raster);
    EXPECT_GT(counter.getCalls(), 0);
    counter.reset();
    opened.state.applyRaster(raster);
    EXPECT_EQ(counter.getCalls(), 0);
}

TEST_P(StateCacheTest, SendsOnlyTheStateThatChanged) {
    StateCache::Raster raster = makeBlendedRaster();
    opened.state.applyRaster(raster);
    GlCallCounter counter(opened.gl);
    watchRaster(counter);

    raster.depthCompare = Gl::kGreater;
    opened.state.applyRaster(raster);
    EXPECT_EQ(counter.getCalls(), 1);
}

// A clear opens the channels a pipeline closed, and the next draw closes them again.
TEST_P(StateCacheTest, ReopensTheChannelsAPipelineClosedForAClear) {
    const StateCache::Raster raster = makeBlendedRaster();
    opened.state.applyRaster(raster);
    GlCallCounter counter(opened.gl);
    counter.watch<&Gl::colorMask>();

    opened.state.prepareClear();
    EXPECT_EQ(counter.getCalls(), 1);
    opened.state.applyRaster(raster);
    EXPECT_EQ(counter.getCalls(), 2);
}

// OpenGL may give the name of a deleted texture to a new one, so the unit binds it again.
TEST_P(StateCacheTest, BindsATextureAgainOnceItsNameIsForgotten) {
    Gl::Uint texture = 0;
    opened.gl.genTextures(1, &texture);
    GlCallCounter counter(opened.gl);
    counter.watch<&Gl::bindTexture>();
    counter.watch<&Gl::activeTexture>();

    opened.state.bindTexture(3, Gl::kTexture2d, texture);
    opened.state.bindTexture(3, Gl::kTexture2d, texture);
    EXPECT_EQ(counter.getCalls(), 2);
    opened.state.forgetTexture(texture);
    opened.state.bindTexture(3, Gl::kTexture2d, texture);
    EXPECT_EQ(counter.getCalls(), 3);
    opened.gl.deleteTextures(1, &texture);
}

INSTANTIATE_TEST_SUITE_P(Flavours, StateCacheTest, ::testing::Values(Device::Api::OpenGl, Device::Api::OpenGlEs), OpenGlTest::getFlavourName);

} // namespace haylen::gpu::opengl
