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
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "2d/graphics/MaterialResource.hpp"
#include "graphics/DeviceState.hpp"
#include "graphics/Gpu.hpp"
#include "graphics/ShaderResource.hpp"
#include "haylen/2d/graphics/Material.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/graphics/Shader.hpp"
#include "support/DrawingScene.hpp"
#include "support/EngineFixture.hpp"
#include "support/TemporaryDirectory.hpp"
#include "support/TestFiles.hpp"

namespace haylen {

namespace {

class ShaderTest : public ::testing::Test {
  protected:
    // A material with two uniform blocks of every kind of member and a texture of its own.
    static constexpr const char* kTintSource = R"(@include haylen/material.glsl

@fs tint_fs
@include_block haylen_fragment
layout(binding=1) uniform tint_params {
    vec4 tint;
    float strength;
    vec2 offset;
    int steps;
    mat4 transform;
    vec4 palette[3];
};
layout(binding=2) uniform tint_extra {
    vec3 glow;
};
layout(binding=1) uniform texture2D mask_texture;
layout(binding=1) uniform sampler mask_sampler;

void main() {
    vec4 base = haylen_base(uv + offset);
    float mask = texture(sampler2D(mask_texture, mask_sampler), uv).r;
    vec4 moved = transform * vec4(base.rgb, 1.0);
    haylen_output(vec4(mix(base.rgb, moved.rgb * tint.rgb + glow + palette[steps].rgb, strength * mask), base.a));
}
@end

@program tint haylen_vs tint_fs
)";

    // A small shader file with a block, a texture and one program for the desktop GLSL of the headless backend, which the malformed cases change field by field.
    static constexpr const char* kSmallShader = R"({"format": "haylen-shader", "version": 1, "name": "small",
    "blocks": [{"name": "params", "slot": 1, "size": 16, "uniforms": [{"name": "tint", "type": "vec4", "count": 1, "offset": 0}]}],
    "textures": [{"name": "mask", "slot": 1}],
    "sources": ["void main() {}"],
    "programs": {"sprite": {"glsl430": {
        "vertex": {"source": 0, "entry": "main"},
        "fragment": {"source": 0, "entry": "main"},
        "attrs": [{"slot": 0, "glsl_name": "corner", "base_type": "float"}],
        "uniform_blocks": [{"slot": 1, "stage": "fragment", "size": 16, "glsl_uniforms": [{"type": "vec4", "array_count": 1, "glsl_name": "params"}]}],
        "views": [{"slot": 1, "stage": "fragment", "sample_type": "float", "multisampled": false, "image_type": "2d"}],
        "samplers": [{"slot": 1, "stage": "fragment", "sampler_type": "filtering"}],
        "texture_sampler_pairs": [{"slot": 0, "stage": "fragment", "view_slot": 1, "sampler_slot": 1, "glsl_name": "mask_sampler"}]}}}})";

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

    // Compiles the material with `haylen.py shaders`, the way apps compile theirs, once for every test that needs it. Python starts without a shell, so no shell reinterprets the quotes of the paths.
    [[nodiscard]] static const std::vector<std::uint8_t>& tintShader() {
        // clang-format off
        static const std::vector<std::uint8_t> compiled = [] {
            const test::TemporaryDirectory app;
            app.write("app.json", R"({"name": "Shaders", "identifier": "dev.haylen.shaders", "version": "1.0.0"})");
            app.write("content/shaders/tint.glsl", kTintSource);
            if (Poco::Process::launch(HAYLEN_PYTHON, {HAYLEN_TOOL_SCRIPT, "shaders", app.getPath().string()}).wait() != 0) {
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

    // The blocks of the material keep the std140 layout sokol-shdc gives them, and the blocks and texture of the shader library stay out.
    ASSERT_EQ(shader.getBlocks().size(), 2U);
    EXPECT_EQ(shader.getBlocks()[0].name, "tint_params");
    EXPECT_EQ(shader.getBlocks()[0].slot, 1);
    EXPECT_EQ(shader.getBlocks()[0].size, 160U);
    EXPECT_EQ(shader.getBlocks()[1].name, "tint_extra");
    EXPECT_EQ(shader.getBlocks()[1].slot, 2);

    using Type = graphics::Shader::UniformType;
    const std::vector<std::string_view> names{"tint", "strength", "offset", "steps", "transform", "palette", "glow"};
    const std::vector<Type> types{Type::Vec4, Type::Float, Type::Vec2, Type::Int, Type::Mat4, Type::Vec4, Type::Vec3};
    const std::vector<std::uint32_t> offsets{0, 16, 24, 32, 48, 112, 0};
    ASSERT_EQ(shader.getUniforms().size(), names.size());
    for (std::size_t index = 0; index < names.size(); ++index) {
        const graphics::Shader::Uniform* uniform = shader.findUniform(names[index]);
        ASSERT_NE(uniform, nullptr) << names[index];
        EXPECT_EQ(uniform->type, types[index]) << names[index];
        EXPECT_EQ(uniform->offset, offsets[index]) << names[index];
    }
    EXPECT_EQ(shader.findUniform("palette")->count, 3);
    EXPECT_EQ(shader.findUniform("glow")->block, 1U);
    EXPECT_EQ(shader.findUniform("sprite_texture"), nullptr);
    ASSERT_EQ(shader.getTextures().size(), 1U);
    EXPECT_EQ(shader.findTexture("mask_texture")->slot, 1);
    EXPECT_EQ(shader.findTexture("sprite_texture"), nullptr);

    EXPECT_EQ(graphics::Shader::uniformTypeFromName("ivec3"), Type::IVec3);
    EXPECT_EQ(graphics::Shader::uniformTypeName(Type::Mat4), "mat4");
    EXPECT_EQ(graphics::Shader::getComponentCount(Type::Mat4), 16);
    EXPECT_FALSE(graphics::Shader::uniformTypeFromName("mat3").has_value());
}

TEST_F(ShaderTest, RejectsFilesThatAreNotShaders) {
    const auto parse = [](const std::string& text) { return graphics::Shader::parse(test::TestFiles::bytes(text)); };
    EXPECT_THROW((void)parse("not json"), std::invalid_argument);
    EXPECT_THROW((void)parse(R"({"format": "haylen-shader", "version": 2})"), std::invalid_argument);
    EXPECT_THROW((void)parse(R"({"format": "haylen-shader", "version": 1, "name": "x", "blocks": [], "textures": [], "sources": [], "programs": [], "extra": 1})"), std::invalid_argument);
    EXPECT_THROW((void)parse(R"({"format": "haylen-shader", "version": 1, "name": "x", "blocks": [], "textures": [], "sources": [], "programs": []})"), std::invalid_argument);
    EXPECT_THROW((void)parse(R"({"format": "haylen-shader", "version": 1, "name": "x", "blocks": [{"name": "b", "slot": 1, "size": 16, "uniforms": [{"name": "u", "type": "mat3", "count": 1, "offset": 0}]}], "textures": [], "sources": [], "programs": {}})"), std::invalid_argument);

    const graphics::Shader empty;
    EXPECT_FALSE(empty.isValid());
    EXPECT_THROW((void)empty.getName(), std::logic_error);
    graphics::Shader target;
    EXPECT_THROW(target.replace(graphics::Shader::parse(tintShader())), std::logic_error);
}

TEST_F(ShaderTest, RejectsProgramsAndReflectionThatBreakTheGpuLimits) {
    const std::string malformed = "The shader file is malformed: ";
    const std::string program = "/programs/sprite/glsl430";
    EXPECT_EQ(parseError(core::Json::object()), "");

    // Every slot, member and source index stays inside the arrays of the shader description.
    EXPECT_EQ(parseError({{program + "/attrs/0/slot", 16}}), malformed + "The vertex attribute slot 16 is out of range.");
    EXPECT_EQ(parseError({{program + "/uniform_blocks/0/slot", 8}}), malformed + "The uniform block slot 8 is out of range.");
    EXPECT_EQ(parseError({{program + "/uniform_blocks/0/glsl_uniforms", core::Json(std::size_t{17}, core::Json{{"type", "float"}, {"array_count", 1}, {"glsl_name", "x"}})}}), malformed + "The uniform block member 16 is out of range.");
    EXPECT_EQ(parseError({{program + "/views/0/slot", 32}}), malformed + "The texture slot 32 is out of range.");
    EXPECT_EQ(parseError({{program + "/samplers/0/slot", 12}}), malformed + "The sampler slot 12 is out of range.");
    EXPECT_EQ(parseError({{program + "/texture_sampler_pairs/0/slot", 32}}), malformed + "The texture sampler pair slot 32 is out of range.");
    EXPECT_EQ(parseError({{program + "/texture_sampler_pairs/0/sampler_slot", 12}}), malformed + "The sampler slot 12 is out of range.");
    EXPECT_EQ(parseError({{program + "/texture_sampler_pairs/0/view_slot", 2}}), malformed + "The texture sampler pair at slot 0 names a texture or sampler the program does not declare.");
    EXPECT_EQ(parseError({{program + "/fragment/source", 1}}), malformed + "The source 1 is out of range.");
    EXPECT_EQ(parseError({{program + "/vertex/source", -1}}), malformed + "The source -1 is out of range.");
    EXPECT_NE(parseError({{program + "/fragment", nullptr}}), "");

    // Names outside the ones sokol-shdc writes fail instead of taking a default.
    EXPECT_EQ(parseError({{program + "/uniform_blocks/0/stage", "compute"}}), malformed + "Shaders do not support the \"compute\" stage.");
    EXPECT_EQ(parseError({{program + "/views/0/image_type", "1d"}}), malformed + "Shaders do not support \"1d\" textures.");
    EXPECT_EQ(parseError({{program + "/views/0/sample_type", "half"}}), malformed + "Shaders do not support textures that sample \"half\".");
    EXPECT_EQ(parseError({{program + "/samplers/0/sampler_type", "anisotropic"}}), malformed + "Shaders do not support \"anisotropic\" samplers.");
    EXPECT_EQ(parseError({{program + "/attrs/0/base_type", "double"}}), malformed + "Shaders do not support vertex attributes of type \"double\".");
    EXPECT_EQ(parseError({{program + "/uniform_blocks/0/glsl_uniforms/0/type", "mat3"}}), malformed + "Shaders do not support uniforms of type \"mat3\".");

    // The GLSL members fill their block, and every program reads the blocks of the reflection with their own size.
    EXPECT_EQ(parseError({{program + "/uniform_blocks/0/glsl_uniforms/0/array_count", 2}}), malformed + "The members of the uniform block at slot 1 do not fill its 16 bytes.");
    EXPECT_EQ(parseError({{program + "/uniform_blocks/0/size", 32}, {program + "/uniform_blocks/0/glsl_uniforms", core::Json::array()}}), malformed + "The \"sprite\" program for \"glsl430\" reads 32 bytes from the block \"params\", which holds 16.");

    // Materials write every uniform inside its block, and the reflection keeps its slots inside the limits.
    EXPECT_EQ(parseError({{"/blocks/0/uniforms/0/offset", 4}}), malformed + "The uniform \"tint\" does not fit in the block \"params\".");
    EXPECT_EQ(parseError({{"/blocks/0/uniforms/0/count", 2}}), malformed + "The uniform \"tint\" does not fit in the block \"params\".");
    EXPECT_EQ(parseError({{"/blocks/0/uniforms/0/count", 0}}), malformed + "The uniform \"tint\" has no elements.");
    EXPECT_EQ(parseError({{"/blocks/0/slot", 8}}), malformed + "The uniform block slot 8 is out of range.");
    EXPECT_EQ(parseError({{"/textures/0/slot", 32}}), malformed + "The texture slot 32 is out of range.");
}

// The sources of every program for the backend of the device, which the Metal backend compiles ahead on the I/O pool, each once, since the programs of a file share stages.
TEST_F(ShaderTest, ListsTheSourcesOfTheActiveBackendOnce) {
    test::EngineFixture fixture;
    const core::Json document = core::Json::parse(tintShader().begin(), tintShader().end());
    const std::vector<std::string> sources = graphics::Shader::parse(tintShader()).getResource()->getBackendSources();
    ASSERT_FALSE(sources.empty());
    for (std::size_t index = 0; index < sources.size(); ++index) {
        EXPECT_EQ(std::count(sources.begin(), sources.end(), sources[index]), 1) << index;
    }
    for (const auto& [program, languages] : document.at("programs").items()) {
        for (const char* stage : {"vertex", "fragment"}) {
            const std::string& source = document.at("sources").at(languages.at("glsl430").at(stage).at("source").get<std::size_t>()).get_ref<const std::string&>();
            EXPECT_NE(std::find(sources.begin(), sources.end(), source), sources.end()) << program << " " << stage;
        }
    }
}

// The renderer makes each of its programs the first time a draw needs it, so starting an engine compiles no shader and a frame of sprites compiles only the sprite program.
TEST_F(ShaderTest, MakesTheProgramsOfTheRendererWhenDrawsFirstNeedThem) {
    test::EngineFixture fixture;
    // clang-format off
    const auto shaders = [&fixture] {
        for (const graphics::Device::Pool& pool : fixture.engine().getGraphics().getPools()) {
            if (pool.name == "shaders") {
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

TEST_F(ShaderTest, ReportsAFullShaderPool) {
    test::EngineFixture fixture;
    graphics::Device& device = fixture.engine().getGraphics();
    std::vector<graphics::Shader> shaders;
    std::string error;
    try {
        while (shaders.size() <= static_cast<std::size_t>(graphics::Gpu::kShaderPoolSize)) {
            shaders.push_back(graphics::Shader::parse(test::TestFiles::bytes(kSmallShader)));
            shaders.back().getResource()->graveyard = device.getState().graveyard;
            (void)shaders.back().getResource()->getProgram("sprite");
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
    const std::uint32_t revision = material.getResource()->revision;
    EXPECT_EQ(revision, 7U);

    EXPECT_EQ(material.get("offset"), (std::vector<float>{3.0F, 4.0F}));
    EXPECT_EQ(material.get("glow").size(), 3U);
    EXPECT_EQ(material.get("transform")[12], 10.0F);

    std::vector<std::uint8_t> bytes;
    material.getResource()->pack(bytes);
    ASSERT_EQ(bytes.size(), 160U + material.getShader().getBlocks()[1].size);
    EXPECT_EQ(floatAt(bytes, 4), 0.25F);
    EXPECT_EQ(floatAt(bytes, 16), 0.75F);
    EXPECT_EQ(floatAt(bytes, 28), 4.0F);
    std::int32_t steps = 0;
    std::memcpy(&steps, bytes.data() + 32, sizeof(steps));
    EXPECT_EQ(steps, 2);
    EXPECT_EQ(floatAt(bytes, 48 + 13 * 4), 20.0F);
    EXPECT_EQ(floatAt(bytes, 112 + 11 * 4), 0.5F);
    EXPECT_EQ(floatAt(bytes, 160 + 8), 0.3F);

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
    const std::string bare = R"({"format": "haylen-shader", "version": 1, "name": "bare", "blocks": [], "textures": [], "sources": [], "programs": {"sprite": {}}})";
    EXPECT_THROW(graphics2d::Material{graphics::Shader::parse(test::TestFiles::bytes(bare))}, std::invalid_argument);
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
    const graphics::RenderTarget target = device.createRenderTarget(64, 64);

    // clang-format off
    fixture.engine().getScenes().replace(std::make_shared<test::DrawingScene>([&](core::Engine& engine) {
        graphics2d::Renderer& renderer = engine.getRenderer2D();
        const graphics2d::DrawOrder order{.material = material};
        for (const bool lit : {false, true}) {
            renderer.beginWorld(graphics2d::Camera{}, {.ambientLight = lit ? std::optional(math::Color::white()) : std::nullopt, .postProcess = graphics2d::PostProcess{.materials = {post, post}}});
            renderer.draw({.texture = texture, .order = order});
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
    EXPECT_GE(stats.drawCalls, 15U);
    EXPECT_EQ(material.get("strength"), std::vector<float>{0.5F});

    // Materials replace the sprite, text and mesh programs only.
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

    EXPECT_EQ(fixture.lua("return shader.name .. ' ' .. #shader.uniforms .. ' ' .. shader.uniforms[1].name .. ' ' .. shader.uniforms[1].type .. ' ' .. shader.uniforms[6].count .. ' ' .. shader.textures[1]"), "tint 7 tint vec4 3 mask_texture");
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
