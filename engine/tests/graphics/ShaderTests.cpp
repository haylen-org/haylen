#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "gpu/NullDevice.hpp"
#include "graphics/DeviceState.hpp"
#include "graphics/ShaderResource.hpp"
#include "haylen/2d/graphics/Material.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/graphics/Shader.hpp"
#include "haylen/graphics/ShaderPackageError.hpp"
#include "support/DrawingScene.hpp"
#include "support/EngineFixture.hpp"
#include "support/MaterialShaders.hpp"
#include "support/TestFiles.hpp"

namespace haylen::graphics {

class ShaderTest : public ::testing::Test {
  protected:
    // A small shader package with a block, a texture and one program for the desktop GLSL and Metal, which the malformed cases change field by field.
    static constexpr const char* kSmallShader = R"({"format": "haylen-shader", "version": 5, "name": "small", "sources": ["void main() {}"],
    "programs": {"sprite": {
        "attributes": [{"location": 0, "name": "corner", "type": "vec2"}],
        "blocks": [{"name": "params", "stage": "fragment", "slot": 1, "size": 16, "members": [{"name": "tint", "type": "vec4", "count": 1, "offset": 0}]}],
        "textures": [{"name": "mask", "stage": "fragment", "slot": 1, "type": "2d", "sampleType": "float"}],
        "samplers": [{"name": "mask_sampler", "stage": "fragment", "slot": 1, "type": "filtering"}],
        "pairs": [{"stage": "fragment", "texture": 1, "sampler": 1, "name": "mask_mask_sampler"}],
        "outputs": 1,
        "targets": {
            "glsl430": {"vertex": {"source": 0, "entry": "main"}, "fragment": {"source": 0, "entry": "main"}, "blocks": {"1": "params"}},
            "msl-macos": {"vertex": {"source": 0, "entry": "main0"}, "fragment": {"source": 0, "entry": "main0"}, "blocks": {"1": 1}, "textures": {"1": 1}, "samplers": {"1": 1}}}}}})";

    using Problem = graphics::ShaderPackageError::Problem;

    // Parses the small shader with the values of `changes` written at their JSON pointers, and returns the rule of the package that it breaks, or nothing when the file parses. Every refusal names the file and the command that compiles it again.
    [[nodiscard]] static std::optional<Problem> parseError(const core::Json& changes) {
        core::Json document = core::Json::parse(kSmallShader);
        for (const auto& [pointer, value] : changes.items()) {
            document[core::Json::json_pointer(pointer)] = value;
        }

        try {
            (void)graphics::Shader::parse(test::TestFiles::bytes(document.dump()), "shaders/small.shader");
        } catch (const graphics::ShaderPackageError& error) {
            EXPECT_STREQ(error.what(), "The shader file \"shaders/small.shader\" is damaged or was made by another version of the Haylen shader compiler. Compile it again with \"haylen app shaders\".");
            return error.getProblem();
        }

        return std::nullopt;
    }
};

TEST_F(ShaderTest, CompilesWithAppShadersIntoEveryProgramWithItsReflection) {
    const graphics::Shader shader = graphics::Shader::parse(test::MaterialShaders::tint());
    EXPECT_EQ(shader.getName(), "tint");
    EXPECT_EQ(shader.getVersion(), 1U);

    for (const std::string_view program : graphics2d::Material::kPrograms) {
        EXPECT_TRUE(shader.hasProgram(program)) << program;
    }

    EXPECT_FALSE(shader.hasProgram("light"));

    // The blocks of the material keep the std140 layout the shader compiler gives them, and the blocks and texture of the shader library stay out.
    ASSERT_EQ(shader.getBlocks().size(), 3U);
    EXPECT_EQ(shader.getBlocks()[0].name, "tint_params");
    EXPECT_EQ(shader.getBlocks()[0].slot, 1);
    EXPECT_EQ(shader.getBlocks()[0].size, 144U);
    EXPECT_EQ(shader.getBlocks()[1].name, "tint_extra");
    EXPECT_EQ(shader.getBlocks()[1].slot, 2);
    EXPECT_EQ(shader.getBlocks()[1].size, 64U);
    EXPECT_EQ(shader.getBlocks()[2].name, "tint_counts");
    EXPECT_EQ(shader.getBlocks()[2].size, 16U);

    using Type = graphics::Shader::UniformType;
    const std::vector<std::string_view> names{"tint", "strength", "offset", "steps", "transform", "palette", "glow", "weights"};
    const std::vector<Type> types{Type::Vec4, Type::Float, Type::Vec2, Type::Int, Type::Mat4, Type::Vec4, Type::Vec3, Type::Float};
    const std::vector<std::uint32_t> offsets{0, 16, 24, 0, 32, 96, 0, 16};
    ASSERT_EQ(shader.getUniforms().size(), names.size());

    for (std::size_t index = 0; index < names.size(); ++index) {
        const graphics::Shader::Uniform* uniform = shader.findUniform(names[index]);
        ASSERT_NE(uniform, nullptr) << names[index];
        EXPECT_EQ(uniform->type, types[index]) << names[index];
        EXPECT_EQ(uniform->offset, offsets[index]) << names[index];
    }

    const graphics::Shader::Uniform* palette = shader.findUniform("palette");
    const graphics::Shader::Uniform* weights = shader.findUniform("weights");
    const graphics::Shader::Uniform* glow = shader.findUniform("glow");
    const graphics::Shader::Uniform* steps = shader.findUniform("steps");
    ASSERT_NE(palette, nullptr);
    ASSERT_NE(weights, nullptr);
    ASSERT_NE(glow, nullptr);
    ASSERT_NE(steps, nullptr);
    EXPECT_EQ(palette->count, 3);
    EXPECT_EQ(weights->count, 3);
    EXPECT_EQ(glow->block, 1U);
    EXPECT_EQ(steps->block, 2U);
    EXPECT_EQ(shader.findUniform("sprite_texture"), nullptr);

    ASSERT_EQ(shader.getTextures().size(), 1U);
    const graphics::Shader::TextureSlot* mask = shader.findTexture("mask_texture");
    ASSERT_NE(mask, nullptr);
    EXPECT_EQ(mask->slot, 1);
    EXPECT_EQ(shader.findTexture("sprite_texture"), nullptr);

    EXPECT_EQ(graphics::Shader::uniformTypeFromName("ivec3"), Type::IVec3);
    EXPECT_EQ(graphics::Shader::uniformTypeName(Type::Mat4), "mat4");
    EXPECT_EQ(graphics::Shader::getComponentCount(Type::Mat4), 16);
    EXPECT_EQ(graphics::Shader::getArrayStride(Type::Float), 16U);
    EXPECT_EQ(graphics::Shader::getArrayStride(Type::Vec3), 16U);
    EXPECT_EQ(graphics::Shader::getArrayStride(Type::Mat4), 64U);
    EXPECT_FALSE(graphics::Shader::uniformTypeFromName("mat3").has_value());
}

TEST_F(ShaderTest, RejectsFilesThatAreNotShaders) {
    // clang-format off
    const auto problem = [](const std::string& text) -> std::optional<Problem> {
        try {
            (void)graphics::Shader::parse(test::TestFiles::bytes(text));
        } catch (const graphics::ShaderPackageError& error) {
            return error.getProblem();
        }

        return std::nullopt;
    };
    // clang-format on
    EXPECT_EQ(problem("not json"), Problem::Structure);
    EXPECT_EQ(problem(R"({"format": "haylen-shader", "version": 1, "name": "x", "sources": [], "programs": {"p": {}}})"), Problem::Version);
    EXPECT_EQ(problem(R"({"format": "haylen-shader", "version": 5, "name": "x", "sources": [], "programs": {}, "extra": 1})"), Problem::Structure);
    EXPECT_EQ(problem(R"({"format": "haylen-shader", "version": 5, "name": "x", "sources": [], "programs": {}})"), Problem::NoPrograms);
    EXPECT_EQ(problem(R"({"format": "haylen-shader", "version": 5, "name": "x", "sources": [], "programs": []})"), Problem::NoPrograms);

    const graphics::Shader empty;
    EXPECT_FALSE(empty.isValid());
    EXPECT_THROW((void)empty.getName(), std::logic_error);
    graphics::Shader target;
    EXPECT_THROW(target.replace(graphics::Shader::parse(test::MaterialShaders::tint())), std::logic_error);
}

TEST_F(ShaderTest, RejectsProgramsAndReflectionThatBreakTheGpuLimits) {
    const std::string program = "/programs/sprite";
    const std::string glsl = program + "/targets/glsl430";
    const std::string metal = program + "/targets/msl-macos";
    EXPECT_EQ(parseError(core::Json::object()), std::nullopt);

    // Every slot and source index stays inside the bind slots of every GPU device and the sources of the package.
    EXPECT_EQ(parseError({{program + "/attributes/0/location", 16}}), Problem::SlotRange);
    EXPECT_EQ(parseError({{program + "/blocks/0/slot", 8}}), Problem::SlotRange);
    EXPECT_EQ(parseError({{program + "/textures/0/slot", 32}}), Problem::SlotRange);
    EXPECT_EQ(parseError({{program + "/samplers/0/slot", 12}}), Problem::SlotRange);
    EXPECT_EQ(parseError({{program + "/pairs/0/texture", 2}}), Problem::UnknownPair);
    EXPECT_EQ(parseError({{glsl + "/fragment/source", 1}}), Problem::MissingSource);
    EXPECT_EQ(parseError({{glsl + "/vertex/source", -1}}), Problem::MissingSource);
    EXPECT_EQ(parseError({{program + "/outputs", -1}}), Problem::Outputs);
    EXPECT_FALSE(parseError({{program + "/outputs", 0}}).has_value());
    EXPECT_TRUE(parseError({{glsl + "/fragment", nullptr}}).has_value());

    // Names outside the ones the shader compiler writes fail instead of taking a default.
    EXPECT_EQ(parseError({{program + "/blocks/0/stage", "compute"}}), Problem::Stage);
    EXPECT_EQ(parseError({{program + "/textures/0/type", "1d"}}), Problem::TextureType);
    EXPECT_EQ(parseError({{program + "/textures/0/sampleType", "half"}}), Problem::SampleType);
    EXPECT_EQ(parseError({{program + "/samplers/0/type", "anisotropic"}}), Problem::SamplerType);
    EXPECT_EQ(parseError({{program + "/blocks/0/members/0/type", "mat3"}}), Problem::UniformType);
    EXPECT_EQ(parseError({{program + "/targets/dxil", core::Json::object()}}), Problem::UnknownLanguage);

    // Blocks hold whole vectors, OpenGL finds each block of the program by a name, and every other language binds every slot.
    EXPECT_EQ(parseError({{program + "/blocks/0/size", 24}}), Problem::BlockSize);
    EXPECT_EQ(parseError({{glsl + "/blocks/1", ""}}), Problem::OpenGlBlock);
    EXPECT_EQ(parseError({{glsl + "/blocks/2", "extra"}}), Problem::OpenGlBlock);
    EXPECT_EQ(parseError({{metal + "/textures", core::Json::object()}}), Problem::MissingBinding);
    EXPECT_EQ(parseError({{metal + "/samplers/1", 256}}), Problem::BindingRange);

    // Materials write every uniform inside its block.
    EXPECT_EQ(parseError({{program + "/blocks/0/members/0/offset", 4}}), Problem::UniformLayout);
    EXPECT_EQ(parseError({{program + "/blocks/0/members/0/count", 2}}), Problem::UniformLayout);
    EXPECT_EQ(parseError({{program + "/blocks/0/members/0/count", 0}}), Problem::UniformLayout);

    // Counts, sizes, offsets and slots read in 64 bits, so a number past 32 bits never wraps around to one that fits.
    EXPECT_EQ(parseError({{program + "/outputs", 4294967297}}), Problem::Outputs);
    EXPECT_EQ(parseError({{program + "/pairs/0/texture", 4294967297}}), Problem::SlotRange);
    EXPECT_EQ(parseError({{program + "/pairs/0/sampler", -4294967295}}), Problem::SlotRange);
    EXPECT_EQ(parseError({{program + "/blocks/0/size", 4294967312}}), Problem::BlockSize);
    EXPECT_EQ(parseError({{program + "/blocks/0/members/0/offset", 4294967296}}), Problem::UniformLayout);
    EXPECT_EQ(parseError({{program + "/blocks/0/members/0/count", 4294967297}}), Problem::UniformLayout);
}

// The renderer makes each of its programs the first time a draw needs it, so starting an engine compiles no shader and a frame of sprites compiles only the sprite program.
TEST_F(ShaderTest, MakesTheProgramsOfTheRendererWhenDrawsFirstNeedThem) {
    test::EngineFixture fixture;
    // clang-format off
    const auto shaders = [&fixture] {
        std::vector<graphics::Device::Pool> pools;
        fixture.engine().getGraphics().readPools(pools);
        for (const graphics::Device::Pool& pool : pools) {
            if (pool.name == "programs") {
                return pool.used;
            }
        }

        return -1;
    };
    // clang-format on
    EXPECT_EQ(shaders(), 0);

    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(4, 4, math::Color::white()));
    // clang-format off
    fixture.engine().getScenes().replace(std::make_shared<test::DrawingScene>([&texture](core::Engine& engine) {
        engine.getRenderer2D().beginScreen();
        engine.getRenderer2D().draw({.texture = texture});
    }));
    // clang-format on
    fixture.frames(1);
    EXPECT_EQ(shaders(), 1);
}

// A package compiled without the language of the device names the program and the command that compiles it again, and only the programs with code in the language of the device start compiling ahead.
TEST_F(ShaderTest, NamesTheLanguageAProgramLacks) {
    const graphics::Shader shader = graphics::Shader::parse(test::TestFiles::bytes(kSmallShader));
    gpu::NullDevice gles(gpu::Device::Language::Glsl300es);
    try {
        (void)shader.getResource()->getProgram(gles, "sprite");
        ADD_FAILURE() << "A program without the language of the device was made.";
    } catch (const std::invalid_argument& error) {
        EXPECT_STREQ(error.what(), "The shader program \"small/sprite\" has no code for the graphics API that this device draws with. Compile it again with \"haylen app shaders\".");
    }

    test::EngineFixture fixture;
    core::JobSystem& jobs = fixture.engine().getJobs();
    shader.getResource()->prepare(gles, jobs);
    EXPECT_EQ(gles.getPreparedPrograms(), 0);
    gpu::NullDevice metal(gpu::Device::Language::MetalMacos);
    shader.getResource()->prepare(metal, jobs);
    EXPECT_EQ(metal.getPreparedPrograms(), 1);
    EXPECT_NE(shader.getResource()->getProgram(metal, "sprite").program.id, 0U);
}

// The SPIR-V target holds each module as the Base64 text of its words and names the descriptor set and the binding of every slot, which reach the device as they are.
TEST_F(ShaderTest, DecodesTheSpirvModulesAndTheirDescriptorSets) {
    const std::string program = "/programs/sprite";
    core::Json document = core::Json::parse(kSmallShader);
    document["sources"].push_back("AwIjBwAAAQA=");
    document[core::Json::json_pointer(program + "/targets/spirv")] = core::Json::parse(R"({"vertex": {"source": 1, "entry": "main"}, "fragment": {"source": 1, "entry": "main"},
        "blocks": {"1": {"set": 0, "binding": 1}}, "textures": {"1": {"set": 1, "binding": 1}}, "samplers": {"1": {"set": 1, "binding": 65}}})");
    const graphics::Shader shader = graphics::Shader::parse(test::TestFiles::bytes(document.dump()));

    gpu::NullDevice vulkan(gpu::Device::Language::Spirv);
    const gpu::NullDevice::ProgramRecord& record = vulkan.getProgramRecord(shader.getResource()->getProgram(vulkan, "sprite").program);
    EXPECT_EQ(record.vertexBytes, 8U);
    EXPECT_EQ(record.fragmentBytes, 8U);
    ASSERT_EQ(record.blocks.size(), 1U);
    EXPECT_EQ(record.blocks.front().set, 0);
    EXPECT_EQ(record.blocks.front().binding, 1);
    ASSERT_EQ(record.textures.size(), 1U);
    EXPECT_EQ(record.textures.front().set, 1);
    EXPECT_EQ(record.textures.front().binding, 1);
    ASSERT_EQ(record.samplers.size(), 1U);
    EXPECT_EQ(record.samplers.front().set, 1);
    EXPECT_EQ(record.samplers.front().binding, 65);

    // The cases below give the small shader one source, which the SPIR-V target reads too.
    core::Json spirv = document.at(core::Json::json_pointer(program + "/targets/spirv"));
    spirv["vertex"]["source"] = 0;
    spirv["fragment"]["source"] = 0;
    EXPECT_EQ(parseError({{"/sources/0", "AwIj"}, {program + "/targets/spirv", spirv}}), Problem::Code);
    EXPECT_EQ(parseError({{"/sources/0", "AwIjBwAAAQA="}, {program + "/targets/spirv", spirv}}), std::nullopt);
    spirv["samplers"]["1"] = 65;
    EXPECT_TRUE(parseError({{"/sources/0", "AwIjBwAAAQA="}, {program + "/targets/spirv", spirv}}).has_value());
}

// A shader that reloads takes the SPIR-V modules of its new package, so the next program runs the edited code.
TEST_F(ShaderTest, ReplacesTheSpirvModulesWhenItReloads) {
    const std::string program = "/programs/sprite";
    core::Json document = core::Json::parse(kSmallShader);
    document["sources"].push_back("AwIjBwAAAQA=");
    document[core::Json::json_pointer(program + "/targets/spirv")] = core::Json::parse(R"({"vertex": {"source": 1, "entry": "main"}, "fragment": {"source": 1, "entry": "main"},
        "blocks": {"1": {"set": 0, "binding": 1}}, "textures": {"1": {"set": 1, "binding": 1}}, "samplers": {"1": {"set": 1, "binding": 65}}})");
    graphics::Shader shader = graphics::Shader::parse(test::TestFiles::bytes(document.dump()));
    document["sources"][1] = "AwIjBwAAAQAAAAAA";
    shader.replace(graphics::Shader::parse(test::TestFiles::bytes(document.dump())));

    gpu::NullDevice vulkan(gpu::Device::Language::Spirv);
    const gpu::NullDevice::ProgramRecord& record = vulkan.getProgramRecord(shader.getResource()->getProgram(vulkan, "sprite").program);
    EXPECT_EQ(record.vertexBytes, 12U);
    EXPECT_EQ(record.fragmentBytes, 12U);
}

// The target of a port holds the code that the shader tool of the port wrote as Base64 text of bytes in any count, with the descriptor set and the binding of every slot, which reach the device of the port as they are.
TEST_F(ShaderTest, DecodesTheCodeOfAPortAndItsBindings) {
    const std::string program = "/programs/sprite";
    core::Json document = core::Json::parse(kSmallShader);
    document["sources"].push_back("UE9SVA==");
    document[core::Json::json_pointer(program + "/targets/port")] = core::Json::parse(R"({"vertex": {"source": 1, "entry": "main"}, "fragment": {"source": 1, "entry": "main"},
        "blocks": {"1": {"set": 2, "binding": 3}}, "textures": {"1": {"set": 4, "binding": 5}}, "samplers": {"1": {"set": 4, "binding": 6}}})");
    const graphics::Shader shader = graphics::Shader::parse(test::TestFiles::bytes(document.dump()));

    gpu::NullDevice port(gpu::Device::Language::Port, gpu::NullDevice::getDefaultCapabilities(), gpu::Device::Api::Port);
    EXPECT_EQ(port.getApi(), gpu::Device::Api::Port);
    const gpu::NullDevice::ProgramRecord& record = port.getProgramRecord(shader.getResource()->getProgram(port, "sprite").program);
    EXPECT_EQ(record.vertexBytes, 4U);
    EXPECT_EQ(record.fragmentBytes, 4U);
    ASSERT_EQ(record.blocks.size(), 1U);
    EXPECT_EQ(record.blocks.front().set, 2);
    EXPECT_EQ(record.blocks.front().binding, 3);
    ASSERT_EQ(record.textures.size(), 1U);
    EXPECT_EQ(record.textures.front().set, 4);
    EXPECT_EQ(record.textures.front().binding, 5);
    ASSERT_EQ(record.samplers.size(), 1U);
    EXPECT_EQ(record.samplers.front().binding, 6);

    core::Json code = document.at(core::Json::json_pointer(program + "/targets/port"));
    code["vertex"]["source"] = 0;
    code["fragment"]["source"] = 0;
    EXPECT_EQ(parseError({{"/sources/0", "@@@"}, {program + "/targets/port", code}}), Problem::Code);
    EXPECT_EQ(parseError({{"/sources/0", "UE9SVA=="}, {program + "/targets/port", code}}), std::nullopt);
}

TEST_F(ShaderTest, ReportsAFullShaderPool) {
    test::EngineFixture fixture;
    graphics::Device& device = fixture.engine().getGraphics();
    std::vector<graphics::Shader> shaders;
    std::string error;
    try {
        while (shaders.size() <= static_cast<std::size_t>(gpu::NullDevice::kProgramPoolSize)) {
            shaders.push_back(graphics::Shader::parse(test::TestFiles::bytes(kSmallShader)));
            shaders.back().getResource()->graveyard = device.getState().graveyard;
            (void)shaders.back().getResource()->getProgram(device.getGpu(), "sprite");
        }
    } catch (const std::runtime_error& failure) {
        error = failure.what();
    }

    EXPECT_EQ(error, "The graphics device has no room for another shader. At most 512 shader programs can exist at once.");

    shaders.clear();
    device.collectGarbage();
}

} // namespace haylen::graphics
