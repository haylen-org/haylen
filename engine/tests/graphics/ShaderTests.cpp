#include <gtest/gtest.h>

#include <Poco/Process.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "2d/graphics/MaterialResource.hpp"
#include "gpu/NullDevice.hpp"
#include "graphics/DeviceState.hpp"
#include "graphics/ShaderResource.hpp"
#include "haylen/2d/graphics/Material.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/SpriteData.hpp"
#include "haylen/2d/graphics/SpriteInstance.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/graphics/Shader.hpp"
#include "support/AllocationTracker.hpp"
#include "support/DrawingScene.hpp"
#include "support/EngineFixture.hpp"
#include "support/TemporaryDirectory.hpp"
#include "support/TestFiles.hpp"

namespace haylen {

namespace {

class ShaderTest : public ::testing::Test {
  protected:
    // A material with uniform blocks of every kind of member, floats and integers in blocks of their own, and a texture of its own.
    static constexpr const char* kTintSource = R"(@include haylen/material.glsl

@fs tint_fs
@include_block haylen_fragment
layout(binding=1) uniform tint_params {
    vec4 tint;
    float strength;
    vec2 offset;
    mat4 transform;
    vec4 palette[3];
};
layout(binding=2) uniform tint_extra {
    vec3 glow;
    float weights[3];
};
layout(binding=3) uniform tint_counts {
    int steps;
};
layout(binding=1) uniform texture2D mask_texture;
layout(binding=1) uniform sampler mask_sampler;

void main() {
    vec4 base = haylen_base(uv + offset);
    float mask = texture(sampler2D(mask_texture, mask_sampler), uv).r;
    vec4 moved = transform * vec4(base.rgb, 1.0);
    haylen_output(vec4(mix(base.rgb, moved.rgb * tint.rgb + glow * weights[steps] + palette[steps].rgb, strength * mask), base.a));
}
@end

@program tint haylen_vs tint_fs
)";

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

    // Compiles the material with `haylen.py shaders`, the way apps compile theirs, with the shader compiler of the build, once for every test that needs it. Python starts without a shell, so no shell reinterprets the quotes of the paths.
    [[nodiscard]] static const std::vector<std::uint8_t>& tintShader() {
        // clang-format off
        static const std::vector<std::uint8_t> compiled = [] {
            const test::TemporaryDirectory app;
            app.write("app.json", R"({"name": "Shaders", "identifier": "com.haylen.shaders", "version": "1.0.0"})");
            app.write("content/shaders/tint.glsl", kTintSource);
            const Poco::Process::Env env{{"HAYLEN_SHADER_COMPILER", HAYLEN_SHADER_COMPILER}};
            if (Poco::Process::launch(HAYLEN_PYTHON, {HAYLEN_TOOL_SCRIPT, "shaders", app.getPath().string()}, "", nullptr, nullptr, nullptr, env).wait() != 0) {
                throw std::runtime_error("The command \"haylen.py shaders\" failed.");
            }
            std::ifstream file(app.getPath() / "content" / "shaders" / "tint.shader", std::ios::binary);
            return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        }();
        // clang-format on
        return compiled;
    }

    [[nodiscard]] static float floatAt(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
        float value = 0.0F;
        std::memcpy(&value, bytes.data() + offset, sizeof(value));
        return value;
    }
};

class MaterialTest : public ShaderTest {};

class MaterialLuaTest : public ShaderTest {};

} // namespace

TEST_F(ShaderTest, CompilesWithMakePyIntoEveryProgramWithItsReflection) {
    const graphics::Shader shader = graphics::Shader::parse(tintShader());
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
    EXPECT_EQ(shader.findUniform("palette")->count, 3);
    EXPECT_EQ(shader.findUniform("weights")->count, 3);
    EXPECT_EQ(shader.findUniform("glow")->block, 1U);
    EXPECT_EQ(shader.findUniform("steps")->block, 2U);
    EXPECT_EQ(shader.findUniform("sprite_texture"), nullptr);
    ASSERT_EQ(shader.getTextures().size(), 1U);
    EXPECT_EQ(shader.findTexture("mask_texture")->slot, 1);
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
    EXPECT_THROW(target.replace(graphics::Shader::parse(tintShader())), std::logic_error);
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
    EXPECT_EQ(parseError({{glsl + "/blocks/1/type", "mat4"}}), malformed + "The block \"params\" is read as \"mat4\", and OpenGL reads blocks as arrays of \"vec4\" or \"ivec4\".");
    EXPECT_EQ(parseError({{metal + "/textures", core::Json::object()}}), malformed + "The texture at slot 1 has no binding number.");
    EXPECT_EQ(parseError({{metal + "/samplers/1", 256}}), malformed + "The binding number 256 of the sampler at slot 1 is out of range.");

    // Materials write every uniform inside its block.
    EXPECT_EQ(parseError({{program + "/blocks/0/members/0/offset", 4}}), malformed + "The uniform \"tint\" does not fit in the block \"params\".");
    EXPECT_EQ(parseError({{program + "/blocks/0/members/0/count", 2}}), malformed + "The uniform \"tint\" does not fit in the block \"params\".");
    EXPECT_EQ(parseError({{program + "/blocks/0/members/0/count", 0}}), malformed + "The uniform \"tint\" does not fit in the block \"params\".");
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

TEST_F(MaterialTest, SetsUniformsByNameAndPacksThemInTheirBlocks) {
    graphics2d::Material material(graphics::Shader::parse(tintShader()));
    EXPECT_TRUE(material.isValid());
    EXPECT_EQ(material.getShader().getName(), "tint");
    EXPECT_EQ(material.get("strength"), std::vector<float>{0.0F});

    material.set("tint", math::Color{0.5F, 0.25F, 1.0F, 1.0F});
    material.set("strength", 0.75F);
    material.set("offset", math::Vec2{3.0F, 4.0F});
    material.set("steps", 2.0F);
    material.set("transform", math::Transform2D::translation({10.0F, 20.0F}));
    material.set("glow", math::Color{0.1F, 0.2F, 0.3F, 1.0F});
    const std::vector<float> palette(12, 0.5F);
    material.set("palette", palette);
    material.set("weights", std::vector<float>{1.0F, 2.0F, 3.0F});
    const std::uint32_t revision = material.getResource()->revision;
    EXPECT_EQ(revision, 8U);

    EXPECT_EQ(material.get("offset"), (std::vector<float>{3.0F, 4.0F}));
    EXPECT_EQ(material.get("glow").size(), 3U);
    EXPECT_EQ(material.get("transform")[12], 10.0F);

    std::vector<std::uint8_t> bytes;
    material.getResource()->pack(bytes);
    ASSERT_EQ(bytes.size(), 144U + 64U + 16U);
    EXPECT_EQ(floatAt(bytes, 4), 0.25F);
    EXPECT_EQ(floatAt(bytes, 16), 0.75F);
    EXPECT_EQ(floatAt(bytes, 28), 4.0F);
    EXPECT_EQ(floatAt(bytes, 32 + 13 * 4), 20.0F);
    EXPECT_EQ(floatAt(bytes, 96 + 11 * 4), 0.5F);
    EXPECT_EQ(floatAt(bytes, 144 + 8), 0.3F);

    // Every element of an array of floats takes a whole vector of four in the std140 layout.
    EXPECT_EQ(floatAt(bytes, 144 + 16), 1.0F);
    EXPECT_EQ(floatAt(bytes, 144 + 32), 2.0F);
    EXPECT_EQ(floatAt(bytes, 144 + 48), 3.0F);
    EXPECT_EQ(floatAt(bytes, 144 + 20), 0.0F);
    std::int32_t steps = 0;
    std::memcpy(&steps, bytes.data() + 208, sizeof(steps));
    EXPECT_EQ(steps, 2);

    test::EngineFixture fixture;
    const graphics::Texture mask = fixture.engine().getGraphics().createTexture(graphics::Image(2, 2, math::Color::white()));
    material.setTexture("mask_texture", mask);
    EXPECT_EQ(material.getTexture("mask_texture"), mask);
    EXPECT_EQ(material.getResource()->getTextureAt(1), mask);
    EXPECT_FALSE(material.getResource()->getTextureAt(4).isValid());

    EXPECT_THROW(material.set("missing", 1.0F), std::invalid_argument);
    EXPECT_THROW(material.set("tint", 1.0F), std::invalid_argument);
    EXPECT_THROW(material.set("strength", math::Vec2{}), std::invalid_argument);
    EXPECT_THROW(material.set("palette", std::vector<float>(4, 0.0F)), std::invalid_argument);
    EXPECT_THROW(material.setTexture("missing", mask), std::invalid_argument);
    EXPECT_THROW((void)material.getTexture("tint"), std::invalid_argument);
    EXPECT_EQ(material.getResource()->revision, revision + 1);

    EXPECT_THROW(graphics2d::Material{graphics::Shader{}}, std::invalid_argument);
    EXPECT_THROW((void)graphics2d::Material{}.getShader(), std::logic_error);
    const std::string bare = R"({"format": "haylen-shader", "version": 2, "name": "bare", "sources": [], "programs": {"sprite": {"attributes": [], "blocks": [], "textures": [], "samplers": [], "pairs": [], "outputs": 1, "targets": {}}}})";
    EXPECT_THROW(graphics2d::Material{graphics::Shader::parse(test::TestFiles::bytes(bare))}, std::invalid_argument);
}

// A value an app sets every frame, such as a time, and the copy every frame packs of it reuse their storage.
TEST_F(MaterialTest, SetsAndPacksValuesEveryFrameWithoutAllocating) {
    graphics2d::Material material(graphics::Shader::parse(tintShader()));
    material.set("strength", 0.5F);
    material.set("palette", std::vector<float>(12, 0.5F));
    std::vector<std::uint8_t> bytes;
    material.getResource()->pack(bytes);

    const test::AllocationTracker tracker;
    for (int frame = 1; frame <= 10; ++frame) {
        material.set("strength", static_cast<float>(frame));
        bytes.clear();
        material.getResource()->pack(bytes);
    }
    EXPECT_EQ(tracker.getTotal(), 0U);
    EXPECT_EQ(floatAt(bytes, 16), 10.0F);
    EXPECT_EQ(floatAt(bytes, 96), 0.5F);
}

TEST_F(MaterialTest, DrawsEveryKindOfDrawAndPostProcessesCanvases) {
    const std::string compiled(tintShader().begin(), tintShader().end());
    test::EngineFixture fixture({{"content/shaders/tint.shader", compiled}});
    graphics::Device& device = fixture.engine().getGraphics();
    const graphics::Shader shader = fixture.engine().getAssets().shader("shaders/tint.shader");
    graphics2d::Material material(shader);
    material.set("strength", 1.0F);
    const graphics2d::Material post(shader);
    const graphics::Texture texture = device.createTexture(graphics::Image(8, 8, math::Color::white()));
    const std::vector<graphics::Texture> textures{texture, device.createTexture(graphics::Image(16, 16, math::Color::white()))};
    const std::vector<graphics2d::SpriteInstance> sprites(2, graphics2d::SpriteInstance{.size = {4.0F, 4.0F}});
    const std::vector<graphics2d::SpriteData> data{{.values = {1.0F}}, {.texture = 1, .values = {0.5F}}};
    const graphics::RenderTarget target = device.createRenderTarget(64, 64);

    // clang-format off
    fixture.engine().getScenes().replace(std::make_shared<test::DrawingScene>([&](core::Engine& engine) {
        graphics2d::Renderer& renderer = engine.getRenderer2D();
        const graphics2d::DrawOrder order{.material = material};
        for (const bool lit : {false, true}) {
            renderer.beginWorld(graphics2d::Camera{}, {.ambientLight = lit ? std::optional(math::Color::white()) : std::nullopt, .postProcess = graphics2d::PostProcess{.materials = {post, post}}});
            renderer.draw({.texture = texture, .order = order});
            renderer.drawBatch(textures, sprites, data, order);
            renderer.drawBatch(std::span(textures).first(1), sprites, std::span(data).first(1), order);
            material.set("strength", 0.5F);
            renderer.draw({.texture = texture, .order = order});
            renderer.drawRect({0.0F, 0.0F, 4.0F, 4.0F}, math::Color::white(), order);
            renderer.drawCircle({0.0F, 0.0F}, 8.0F, math::Color::white(), order);
            renderer.drawText(*engine.getDefaultFont(), "Hi", {}, {}, order);
        }
        renderer.beginTarget(target, graphics2d::Camera{}, {.ambientLight = math::Color::white(), .postProcess = graphics2d::PostProcess{.materials = {post}}});
        renderer.draw({.texture = texture, .order = order});
    }));
    // clang-format on
    fixture.frames(2);

    // Each canvas copies the material once per change of its values, so both draws of a canvas keep their own strength.
    const graphics2d::Renderer::Stats& stats = fixture.engine().getRenderer2D().getStats();
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_EQ(stats.canvases, 3U);
    EXPECT_GE(stats.drawCalls, 19U);
    EXPECT_EQ(material.get("strength"), std::vector<float>{0.5F});

    // Materials replace the sprite, batch, text, mesh and shape programs only.
    graphics2d::Renderer& renderer = fixture.engine().getRenderer2D();
    renderer.beginScreen();
    EXPECT_THROW(renderer.drawImageBlend({.from = texture, .to = texture}, {.material = material}), std::invalid_argument);
    EXPECT_THROW(renderer.drawMetaballs(std::vector<math::Vec2>{{}}, 4.0F, {}, {.material = material}), std::invalid_argument);
    fixture.frames(1);
}

TEST_F(MaterialTest, ReloadsItsShaderInPlace) {
    const std::string compiled(tintShader().begin(), tintShader().end());
    test::EngineFixture fixture({{"content/shaders/tint.shader", compiled}});
    assets::Manager& assets = fixture.engine().getAssets();
    const graphics::Shader shader = assets.shader("shaders/tint.shader");
    graphics2d::Material material(shader);
    material.set("strength", 0.25F);
    EXPECT_EQ(assets.shader("shaders/tint.shader"), shader);

    // A reload keeps every handle and the values that still fit, and the next draws create the programs again.
    fixture.package().setFile("content/shaders/tint.shader", test::TestFiles::bytes(compiled));
    EXPECT_EQ(assets.reload("shaders/tint.shader"), 1U);
    EXPECT_EQ(shader.getVersion(), 2U);
    EXPECT_EQ(material.get("strength"), std::vector<float>{0.25F});
    fixture.package().setFile("content/shaders/tint.shader", test::TestFiles::bytes("{}"));
    EXPECT_THROW((void)assets.reload("shaders/tint.shader"), std::invalid_argument);
    EXPECT_EQ(shader.getVersion(), 2U);
    EXPECT_THROW((void)assets.load("shader", "shaders/tint.shader", core::Json{{"filter", "linear"}}), std::invalid_argument);
}

TEST_F(MaterialLuaTest, LoadsShadersAndDrawsWithMaterials) {
    const std::string compiled(tintShader().begin(), tintShader().end());
    test::EngineFixture fixture({{"content/shaders/tint.shader", compiled}});
    fixture.runLua(R"(
        assets = require('haylen.assets')
        graphics = require('haylen.graphics')
        graphics2d = require('haylen.graphics2d')
        m = require('haylen.math')
        shader = assets.shader('shaders/tint.shader')
        tint = graphics2d.newMaterial(shader, {tint = '#FF8040FF', strength = 0.5, offset = {1, 2}, steps = 1, transform = m.identity(), glow = {r = 1, g = 0.5, b = 0}, palette = {1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1}, mask_texture = graphics.whiteTexture()})
    )");

    EXPECT_EQ(fixture.lua("return shader.name .. ' ' .. #shader.uniforms .. ' ' .. shader.uniforms[1].name .. ' ' .. shader.uniforms[1].type .. ' ' .. shader.uniforms[5].count .. ' ' .. shader.textures[1]"), "tint 8 tint vec4 3 mask_texture");
    EXPECT_EQ(fixture.lua("return tostring(assets.load('shaders/tint.shader') == shader) .. ' ' .. tostring(tint.shader == shader)"), "true true");
    EXPECT_EQ(fixture.lua("return tint:get('strength') .. ' ' .. tint:get('offset').y .. ' ' .. tint:get('steps') .. ' ' .. string.format('%.2f', tint:get('tint').g) .. ' ' .. #tint:get('glow') .. ' ' .. #tint:get('palette') .. ' ' .. tint:get('mask_texture').width"), "0.5 2.0 1 0.25 3 12 1");
    EXPECT_EQ(fixture.lua("tint:set('offset', m.vec2(5, 6)) tint:set('tint', {1, 1, 1, 0.5}) tint:set('mask_texture', nil) return tint:get('offset').x .. ' ' .. tint:get('tint').a .. ' ' .. tostring(tint:get('mask_texture'))"), "5.0 0.5 nil");

    fixture.runLua(R"(
        require('haylen.scene').push({render = function()
            local camera = graphics2d.newCamera()
            graphics2d.beginWorld(camera, {ambientLight = '#FF808080', postProcess = {materials = {tint}}})
            graphics2d.draw(graphics.whiteTexture(), 0, 0, {width = 10, height = 10, material = tint})
            local sprite = graphics2d.newSprite(graphics.whiteTexture(), {material = tint})
            sprite:draw()
            spriteMaterial = sprite.material
            graphics2d.drawText(nil, 'Hi', 0, 0, {material = tint})
        end})
    )");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return tostring(spriteMaterial == tint) .. ' ' .. graphics2d.stats().canvases"), "true 1");

    EXPECT_NE(fixture.lua("graphics2d.newMaterial(shader, {missing = 1})").find("has no uniform named \"missing\""), std::string::npos);
    EXPECT_NE(fixture.lua("tint:set('tint', 1)").find("takes 4 numbers, not 1"), std::string::npos);
    EXPECT_NE(fixture.lua("tint:get('nothing')").find("has no uniform named \"nothing\""), std::string::npos);
    EXPECT_NE(fixture.lua("graphics2d.newMaterial(shader, {[1] = 2})").find("uniform names must be strings"), std::string::npos);
    EXPECT_NE(fixture.lua("assets.shader('shaders/missing.shader')").find("error: "), std::string::npos);
}

} // namespace haylen
