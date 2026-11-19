#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
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
#include "support/DrawingScene.hpp"
#include "support/EngineFixture.hpp"
#include "support/MaterialShaders.hpp"
#include "support/TestFiles.hpp"

namespace haylen::graphics {

class ShaderTest : public ::testing::Test {
  protected:
    // A small shader package with a block, a texture and one program for the desktop GLSL and Metal, which the malformed cases change field by field.
    static constexpr const char* kSmallShader = R"({"format": "haylen-shader", "version": 2, "name": "small", "sources": ["void main() {}"],
    "programs": {"sprite": {
        "attributes": [{"location": 0, "name": "corner", "type": "vec2"}],
        "blocks": [{"name": "params", "stage": "fragment", "slot": 1, "size": 16, "members": [{"name": "tint", "type": "vec4", "count": 1, "offset": 0}]}],
        "textures": [{"name": "mask", "stage": "fragment", "slot": 1, "type": "2d", "sampleType": "float"}],
        "samplers": [{"name": "mask_sampler", "stage": "fragment", "slot": 1, "type": "filtering"}],
        "pairs": [{"stage": "fragment", "texture": 1, "sampler": 1, "name": "mask_mask_sampler"}],
        "outputs": 1,
        "targets": {
            "glsl430": {"vertex": {"source": 0, "entry": "main"}, "fragment": {"source": 0, "entry": "main"}, "blocks": {"1": {"name": "params", "type": "vec4", "count": 1}}},
            "msl-macos": {"vertex": {"source": 0, "entry": "main0"}, "fragment": {"source": 0, "entry": "main0"}, "blocks": {"1": 1}, "textures": {"1": 1}, "samplers": {"1": 1}}}}}})";

    // Parses the small shader with the values of `changes` written at their JSON pointers, and returns the error, which is empty when the file parses.
    [[nodiscard]] static std::string parseError(const core::Json& changes) {
        core::Json document = core::Json::parse(kSmallShader);
        for (const auto& [pointer, value] : changes.items()) {
            document[core::Json::json_pointer(pointer)] = value;
        }

        try {
            (void)graphics::Shader::parse(test::TestFiles::bytes(document.dump()));
        } catch (const std::invalid_argument& error) {
            return error.what();
        }

        return {};
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
    const auto parse = [](const std::string& text) { return graphics::Shader::parse(test::TestFiles::bytes(text)); };
    EXPECT_THROW((void)parse("not json"), std::invalid_argument);
    EXPECT_THROW((void)parse(R"({"format": "haylen-shader", "version": 1, "name": "x", "sources": [], "programs": {"p": {}}})"), std::invalid_argument);
    EXPECT_THROW((void)parse(R"({"format": "haylen-shader", "version": 2, "name": "x", "sources": [], "programs": {}, "extra": 1})"), std::invalid_argument);
    EXPECT_THROW((void)parse(R"({"format": "haylen-shader", "version": 2, "name": "x", "sources": [], "programs": {}})"), std::invalid_argument);
    EXPECT_THROW((void)parse(R"({"format": "haylen-shader", "version": 2, "name": "x", "sources": [], "programs": []})"), std::invalid_argument);

    const graphics::Shader empty;
    EXPECT_FALSE(empty.isValid());
    EXPECT_THROW((void)empty.getName(), std::logic_error);
    graphics::Shader target;
    EXPECT_THROW(target.replace(graphics::Shader::parse(test::MaterialShaders::tint())), std::logic_error);
}

TEST_F(ShaderTest, RejectsProgramsAndReflectionThatBreakTheGpuLimits) {
    const std::string malformed = "The shader file is malformed: ";
    const std::string program = "/programs/sprite";
    const std::string glsl = program + "/targets/glsl430";
    const std::string metal = program + "/targets/msl-macos";
    EXPECT_EQ(parseError(core::Json::object()), "");

    // Every slot and source index stays inside the bind slots of every GPU device and the sources of the package.
    EXPECT_EQ(parseError({{program + "/attributes/0/location", 16}}), malformed + "The attribute slot 16 is out of range.");
    EXPECT_EQ(parseError({{program + "/blocks/0/slot", 8}}), malformed + "The uniform block slot 8 is out of range.");
    EXPECT_EQ(parseError({{program + "/textures/0/slot", 32}}), malformed + "The texture slot 32 is out of range.");
    EXPECT_EQ(parseError({{program + "/samplers/0/slot", 12}}), malformed + "The sampler slot 12 is out of range.");
    EXPECT_EQ(parseError({{program + "/pairs/0/texture", 2}}), malformed + "The texture sampler pair \"mask_mask_sampler\" names a texture or sampler the program does not declare.");
    EXPECT_EQ(parseError({{glsl + "/fragment/source", 1}}), malformed + "A stage names the source 1, which the file does not have.");
    EXPECT_EQ(parseError({{glsl + "/vertex/source", -1}}), malformed + "A stage names the source -1, which the file does not have.");
    EXPECT_EQ(parseError({{program + "/outputs", 0}}), malformed + "The program \"sprite\" writes 0 colors, and draws write one to 4.");
    EXPECT_NE(parseError({{glsl + "/fragment", nullptr}}), "");

    // Names outside the ones the shader compiler writes fail instead of taking a default.
    EXPECT_EQ(parseError({{program + "/blocks/0/stage", "compute"}}), malformed + "Shaders have no \"compute\" stage.");
    EXPECT_EQ(parseError({{program + "/textures/0/type", "1d"}}), malformed + "Shaders have no \"1d\" textures.");
    EXPECT_EQ(parseError({{program + "/textures/0/sampleType", "half"}}), malformed + "Shaders have no textures that sample \"half\".");
    EXPECT_EQ(parseError({{program + "/samplers/0/type", "anisotropic"}}), malformed + "Shaders have no \"anisotropic\" samplers.");
    EXPECT_EQ(parseError({{program + "/blocks/0/members/0/type", "mat3"}}), malformed + "The uniform \"tint\" has the type \"mat3\", which materials do not set.");
    EXPECT_EQ(parseError({{program + "/targets/dxil", core::Json::object()}}), malformed + "The program \"sprite\" has code for \"dxil\", which is not a shader language of the engine.");

    // Blocks hold whole vectors, OpenGL reads each one as an array that fills it, and every other language binds every slot.
    EXPECT_EQ(parseError({{program + "/blocks/0/size", 24}}), malformed + "The uniform block \"params\" holds 24 bytes, which is not a multiple of 16.");
    EXPECT_EQ(parseError({{glsl + "/blocks/1/count", 2}}), malformed + "The array of the block \"params\" does not fill its 16 bytes.");
    EXPECT_EQ(parseError({{glsl + "/blocks/1/count", 268435457}}), malformed + "The array of the block \"params\" does not fill its 16 bytes.");
    EXPECT_EQ(parseError({{glsl + "/blocks/1/type", "mat4"}}), malformed + "The block \"params\" is read as \"mat4\", and OpenGL reads blocks as arrays of \"vec4\" or \"ivec4\".");
    EXPECT_EQ(parseError({{metal + "/textures", core::Json::object()}}), malformed + "The texture at slot 1 has no binding number.");
    EXPECT_EQ(parseError({{metal + "/samplers/1", 256}}), malformed + "The binding number 256 of the sampler at slot 1 is out of range.");

    // Materials write every uniform inside its block.
    EXPECT_EQ(parseError({{program + "/blocks/0/members/0/offset", 4}}), malformed + "The uniform \"tint\" does not fit in the block \"params\".");
    EXPECT_EQ(parseError({{program + "/blocks/0/members/0/count", 2}}), malformed + "The uniform \"tint\" does not fit in the block \"params\".");
    EXPECT_EQ(parseError({{program + "/blocks/0/members/0/count", 0}}), malformed + "The uniform \"tint\" does not fit in the block \"params\".");

    // Counts, sizes, offsets and slots read in 64 bits, so a number past 32 bits never wraps around to one that fits.
    EXPECT_EQ(parseError({{program + "/outputs", 4294967297}}), malformed + "The program \"sprite\" writes 4294967297 colors, and draws write one to 4.");
    EXPECT_EQ(parseError({{program + "/pairs/0/texture", 4294967297}}), malformed + "The texture slot 4294967297 is out of range.");
    EXPECT_EQ(parseError({{program + "/pairs/0/sampler", -4294967295}}), malformed + "The sampler slot -4294967295 is out of range.");
    EXPECT_EQ(parseError({{program + "/blocks/0/size", 4294967312}}), malformed + "The uniform block \"params\" holds 4294967312 bytes, more than a block holds.");
    EXPECT_EQ(parseError({{program + "/blocks/0/members/0/offset", 4294967296}}), malformed + "The uniform \"tint\" does not fit in the block \"params\".");
    EXPECT_EQ(parseError({{program + "/blocks/0/members/0/count", 4294967297}}), malformed + "The uniform \"tint\" does not fit in the block \"params\".");
}

// The renderer makes each of its programs the first time a draw needs it, so starting an engine compiles no shader and a frame of sprites compiles only the sprite program.
TEST_F(ShaderTest, MakesTheProgramsOfTheRendererWhenDrawsFirstNeedThem) {
    test::EngineFixture fixture;
    // clang-format off
    const auto shaders = [&fixture] {
        for (const graphics::Device::Pool& pool : fixture.engine().getGraphics().getPools()) {
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

// A package compiled without the language of the device names the program and the language it lacks, and only the programs with code in the language of the device start compiling ahead.
TEST_F(ShaderTest, NamesTheLanguageAProgramLacks) {
    const graphics::Shader shader = graphics::Shader::parse(test::TestFiles::bytes(kSmallShader));
    gpu::NullDevice gles(gpu::Device::Language::Glsl300es);
    try {
        (void)shader.getResource()->getProgram(gles, "sprite");
        ADD_FAILURE() << "A program without the language of the device was made.";
    } catch (const std::invalid_argument& error) {
        EXPECT_STREQ(error.what(), "The program \"small/sprite\" has no code for the \"glsl300es\" language of this GPU device. Compile its shader again with the Haylen shader compiler.");
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
    const std::string malformed = "The shader file is malformed: ";
    EXPECT_EQ(parseError({{"/sources/0", "AwIj"}, {program + "/targets/spirv", spirv}}), malformed + "The source 0 is not the Base64 text of a SPIR-V module.");
    EXPECT_EQ(parseError({{"/sources/0", "AwIjBwAAAQA="}, {program + "/targets/spirv", spirv}}), "");
    spirv["samplers"]["1"] = 65;
    EXPECT_NE(parseError({{"/sources/0", "AwIjBwAAAQA="}, {program + "/targets/spirv", spirv}}), "");
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
    EXPECT_EQ(parseError({{"/sources/0", "@@@"}, {program + "/targets/port", code}}), "The shader file is malformed: The source 0 is not the Base64 text of the code of a port.");
    EXPECT_EQ(parseError({{"/sources/0", "UE9SVA=="}, {program + "/targets/port", code}}), "");
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
