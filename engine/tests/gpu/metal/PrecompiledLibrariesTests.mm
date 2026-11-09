#import <Metal/Metal.h>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

#include "core/EmbeddedFiles.hpp"
#include "gpu/metal/PrecompiledLibraries.hpp"
#include "gpu/support/GpuFixture.hpp"
#include "graphics/ShaderResource.hpp"
#include "haylen/graphics/Shader.hpp"

namespace haylen::gpu {

class GpuPrecompiledLibrariesTest : public test::GpuFixture {
  protected:
    // Whether the build holds a library for the stage that Metal loads and that has the entry point of the stage.
    static ::testing::AssertionResult loads(id<MTLDevice> metal, const graphics::ShaderResource& resource, const graphics::ShaderResource::StageCode& stage) {
        const metal::PrecompiledLibraries::Library* library = metal::PrecompiledLibraries::find(resource.sources[stage.source]);
        if (library == nullptr) {
            return ::testing::AssertionFailure() << "The build holds no library for the source " << stage.source << ".";
        }

        dispatch_data_t code = dispatch_data_create(library->code.data(), library->code.size(), nil, ^{});
        NSError* failure = nil;
        id<MTLLibrary> loaded = [metal newLibraryWithData:code error:&failure];
        if (loaded == nil) {
            return ::testing::AssertionFailure() << "Metal does not load the library of the source " << stage.source << ": " << (failure == nil ? "no reason" : failure.localizedDescription.UTF8String);
        }
        if ([loaded newFunctionWithName:@(stage.entry.c_str())] == nil) {
            return ::testing::AssertionFailure() << "The library of the source " << stage.source << " has no function \"" << stage.entry << "\".";
        }
        return ::testing::AssertionSuccess();
    }
};

// Every stage of every program of the renderer loads from a library of the build, so no app compiles the Metal source of the renderer when it starts.
TEST_P(GpuPrecompiledLibrariesTest, HoldsALibraryForEveryStageOfTheRenderer) {
    if (GetParam() != gpu::Device::Api::Metal) {
        GTEST_SKIP() << "Only the Metal device loads precompiled libraries.";
    }

    const std::array<std::span<const std::uint8_t>, 11> packages{
        core::EmbeddedFiles::getSpriteShader(), core::EmbeddedFiles::getTextShader(), core::EmbeddedFiles::getMeshShader(), core::EmbeddedFiles::getBlendShader(), core::EmbeddedFiles::getCompositeShader(), core::EmbeddedFiles::getLightShader(), core::EmbeddedFiles::getMetaballShader(), core::EmbeddedFiles::getRecolorShader(), core::EmbeddedFiles::getEffectShader(), core::EmbeddedFiles::getShapeShader(), core::EmbeddedFiles::getBackdropShader(),
    };
    const auto language = static_cast<std::size_t>(device().getLanguage());
    id<MTLDevice> metal = MTLCreateSystemDefaultDevice();
    ASSERT_NE(metal, nil);

    int stages = 0;
    for (const std::span<const std::uint8_t> bytes : packages) {
        const graphics::Shader shader = graphics::Shader::parse(bytes);
        const std::shared_ptr<graphics::ShaderResource>& resource = shader.getResource();
        for (const auto& [name, info] : resource->programs) {
            ASSERT_TRUE(info.targets[language].has_value()) << "The program \"" << resource->name << "/" << name << "\" has no Metal code.";
            EXPECT_TRUE(loads(metal, *resource, info.targets[language]->vertex)) << "In the vertex stage of \"" << resource->name << "/" << name << "\".";
            EXPECT_TRUE(loads(metal, *resource, info.targets[language]->fragment)) << "In the fragment stage of \"" << resource->name << "/" << name << "\".";
            stages += 2;
        }
    }
    EXPECT_GT(stages, 0);
}

INSTANTIATE_TEST_SUITE_P(Devices, GpuPrecompiledLibrariesTest, ::testing::ValuesIn(test::GpuFixture::getTestedApis()), test::GpuFixture::nameApi);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GpuPrecompiledLibrariesTest);

} // namespace haylen::gpu
