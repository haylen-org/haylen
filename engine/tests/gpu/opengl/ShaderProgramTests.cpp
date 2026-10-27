#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <string>

#include "gpu/opengl/Context.hpp"
#include "gpu/opengl/GlCallCounter.hpp"
#include "gpu/opengl/OpenGlTest.hpp"
#include "gpu/opengl/ShaderProgram.hpp"

namespace haylen::gpu::opengl {

class ShaderProgramTest : public OpenGlTest {
  protected:
    void SetUp() override {
        OpenGlTest::SetUp();
        if (IsSkipped()) {
            return;
        }
        opened.gl.load(*context, GetParam());
        opened.state.reset();
        const std::string header = GetParam() == Device::Api::OpenGl ? "#version 430\n" : "#version 300 es\nprecision highp float;\n";
        vertex = header + "uniform vec4 tint[2];\nin vec2 position;\nout vec4 shade;\nvoid main() { shade = tint[0] + tint[1]; gl_Position = vec4(position, 0.0, 1.0); }\n";
        fragment = header + "uniform highp sampler2D picture;\nin vec4 shade;\nout vec4 color;\nvoid main() { color = shade * texture(picture, vec2(0.5)); }\n";
    }

    [[nodiscard]] Device::ProgramDesc describe() const {
        return {.vertex = {.source = vertex}, .fragment = {.source = fragment}, .attributes = attributes, .blocks = blocks, .textures = textures, .samplers = samplers, .pairs = pairs, .label = "tinted"};
    }

    Context opened;
    std::string vertex;
    std::string fragment;
    std::array<Device::Attribute, 1> attributes{{{.location = 0, .name = "position"}}};
    std::array<Device::BlockBinding, 1> blocks{{{.slot = 2, .stage = Device::Stage::Vertex, .size = 32, .glslName = "tint", .glslCount = 2}}};
    std::array<Device::TextureBinding, 1> textures{{{.slot = 0}}};
    std::array<Device::SamplerBinding, 1> samplers{{{.slot = 0}}};
    std::array<Device::TexturePair, 1> pairs{{{.texture = 0, .sampler = 0, .glslName = "picture"}}};
};

// A new program stays in use after it sets its units, and the NVIDIA driver warns at a clear when the units of the program in use hold textures it cannot sample, such as a color target made after a program with a shadow sampler, so a clear runs with no program and the next draw uses its own again.
TEST_P(ShaderProgramTest, LeavesNoProgramInUseForAClear) {
    const ShaderProgram program(opened.gl, opened.state, describe());
    GlCallCounter counter(opened.gl);
    counter.watch<&Gl::useProgram>();

    opened.state.prepareClear();
    EXPECT_EQ(counter.getCalls(), 1);
    opened.state.prepareClear();
    EXPECT_EQ(counter.getCalls(), 1);
    opened.state.useProgram(program.getName());
    EXPECT_EQ(counter.getCalls(), 2);
}

TEST_P(ShaderProgramTest, SendsAUniformBlockOnlyWhenItsBytesChange) {
    ShaderProgram program(opened.gl, opened.state, describe());
    opened.state.useProgram(program.getName());
    std::array<float, 8> values{1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.5F, 0.0F, 0.0F};
    GlCallCounter counter(opened.gl);
    counter.watch<&Gl::uniform4fv>();

    program.applyUniforms(2, std::as_bytes(std::span(values)));
    program.applyUniforms(2, std::as_bytes(std::span(values)));
    EXPECT_EQ(counter.getCalls(), 1);
    values[5] = 1.0F;
    program.applyUniforms(2, std::as_bytes(std::span(values)));
    EXPECT_EQ(counter.getCalls(), 2);
    EXPECT_THROW(program.applyUniforms(2, std::as_bytes(std::span(values)).first(16)), std::logic_error);
    EXPECT_THROW(program.applyUniforms(0, std::as_bytes(std::span(values))), std::logic_error);
}

TEST_P(ShaderProgramTest, GivesEachTexturePairTheUnitOfItsIndex) {
    const ShaderProgram program(opened.gl, opened.state, describe());
    ASSERT_EQ(program.getUnits().size(), 1U);
    EXPECT_EQ(program.getUnits()[0].texture, 0U);
    EXPECT_EQ(program.getUnits()[0].target, Gl::kTexture2d);
}

TEST_P(ShaderProgramTest, NamesTheStageThatDidNotCompile) {
    fragment = "#version 300 es\nvoid main() { broken }\n";
    try {
        const ShaderProgram program(opened.gl, opened.state, describe());
        FAIL() << "The program compiled.";
    } catch (const std::runtime_error& error) {
        EXPECT_TRUE(std::string(error.what()).starts_with("The fragment stage of the program \"tinted\" did not compile:")) << error.what();
    }
}

TEST_P(ShaderProgramTest, RefusesToSampleAMultisampledTexture) {
    textures[0].multisampled = true;
    EXPECT_THROW(ShaderProgram(opened.gl, opened.state, describe()), std::logic_error);
}

INSTANTIATE_TEST_SUITE_P(Flavours, ShaderProgramTest, ::testing::Values(Device::Api::OpenGl, Device::Api::OpenGlEs), OpenGlTest::getFlavourName);

} // namespace haylen::gpu::opengl
