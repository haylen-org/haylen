#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

#include "core/EmbeddedFiles.hpp"
#include "gpu/support/GpuFixture.hpp"
#include "graphics/ResourceGraveyard.hpp"
#include "graphics/ShaderResource.hpp"
#include "haylen/graphics/Shader.hpp"

namespace haylen::gpu {

class GpuShaderPackageTest : public test::GpuFixture {};

// The code that the shader compiler writes for the language of each device compiles, links and fits the bindings of the device for every program and variant of the renderer, including the ones no reference scene draws.
TEST_P(GpuShaderPackageTest, MakesEveryProgramOfTheRenderer) {
    const std::array<std::span<const std::uint8_t>, 11> packages{
        core::EmbeddedFiles::getSpriteShader(), core::EmbeddedFiles::getTextShader(), core::EmbeddedFiles::getMeshShader(), core::EmbeddedFiles::getBlendShader(), core::EmbeddedFiles::getCompositeShader(), core::EmbeddedFiles::getLightShader(), core::EmbeddedFiles::getMetaballShader(), core::EmbeddedFiles::getRecolorShader(), core::EmbeddedFiles::getEffectShader(), core::EmbeddedFiles::getShapeShader(), core::EmbeddedFiles::getBackdropShader(),
    };
    const auto programGraveyard = std::make_shared<graphics::ResourceGraveyard>();
    int programs = 0;
    for (const std::span<const std::uint8_t> bytes : packages) {
        const graphics::Shader shader = graphics::Shader::parse(bytes);
        const std::shared_ptr<graphics::ShaderResource>& resource = shader.getResource();
        resource->graveyard = programGraveyard;
        for (const auto& [name, info] : resource->programs) {
            EXPECT_NO_THROW(static_cast<void>(resource->getProgram(device(), name))) << "The program \"" << resource->name << "/" << name << "\" did not make.";
            ++programs;
        }
    }
    programGraveyard->collect(device());
    device().commit();
    EXPECT_GT(programs, 0);
}

INSTANTIATE_TEST_SUITE_P(Devices, GpuShaderPackageTest, ::testing::ValuesIn(test::GpuFixture::getTestedApis()), test::GpuFixture::nameApi);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GpuShaderPackageTest);

} // namespace haylen::gpu
