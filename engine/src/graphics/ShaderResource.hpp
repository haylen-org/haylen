#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "graphics/ResourceGraveyard.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/gpu/Device.hpp"
#include "haylen/graphics/Shader.hpp"

namespace haylen::core {
class JobSystem;
}

namespace haylen::graphics {

// The contents of a shader package that the Haylen shader compiler writes: the reflection and the code of every program for every shader language, read and checked once, and the GPU programs and pipelines made from them on the frame thread, which go to the device graveyard when the contents change or the last handle goes away.
struct ShaderResource {
    // The names of the shader languages as the package writes them, in the order of `gpu::Device::Language`, the first naming none.
    static constexpr std::array<std::string_view, 9> kLanguageNames{"", "msl-macos", "msl-ios", "msl-ios-simulator", "hlsl5", "glsl430", "glsl300es", "wgsl", "spirv"};

    struct StageCode {
        std::size_t source = 0;
        std::string entry;
        std::string profile;
    };

    // A uniform block that the OpenGL languages read as one array of vectors.
    struct GlslBlock {
        int slot = 0;
        std::string name;
        bool integers = false;
        int count = 0;
    };

    // The code of a program in one shader language, with the binding number that the language gives every slot.
    struct Target {
        StageCode vertex;
        StageCode fragment;
        std::vector<std::pair<int, int>> blockBindings;
        std::vector<std::pair<int, int>> textureBindings;
        std::vector<std::pair<int, int>> samplerBindings;
        std::vector<GlslBlock> glslBlocks;
    };

    struct Block {
        std::string name;
        gpu::Device::Stage stage = gpu::Device::Stage::Vertex;
        int slot = 0;
        std::uint32_t size = 0;
    };

    struct TextureInfo {
        std::string name;
        gpu::Device::Stage stage = gpu::Device::Stage::Fragment;
        int slot = 0;
        gpu::Device::TextureType type = gpu::Device::TextureType::Flat;
        gpu::Device::SampleType sampleType = gpu::Device::SampleType::Float;
    };

    struct SamplerInfo {
        std::string name;
        gpu::Device::Stage stage = gpu::Device::Stage::Fragment;
        int slot = 0;
        gpu::Device::SamplerType type = gpu::Device::SamplerType::Filtering;
    };

    struct Pair {
        gpu::Device::Stage stage = gpu::Device::Stage::Fragment;
        int texture = 0;
        int sampler = 0;
        std::string name;
    };

    struct Attribute {
        int location = 0;
        std::string name;
    };

    // One program of the package: its vertex inputs, its bindings and its code for each shader language it was compiled for.
    struct ProgramInfo {
        std::vector<Attribute> attributes;
        std::vector<Block> blocks;
        std::vector<TextureInfo> textures;
        std::vector<SamplerInfo> samplers;
        std::vector<Pair> pairs;
        int outputs = 1;
        std::array<std::optional<Target>, kLanguageNames.size()> targets;

        [[nodiscard]] const Attribute* findAttribute(std::string_view attribute) const noexcept;
    };

    // A GPU program with the size of the uniform block at every bind slot, zero where it has none, and the bind slots it declares, which draws fill and leave the others empty: its fragment uniform blocks, its textures, and its samplers with the texture each one samples.
    struct Compiled {
        gpu::Device::Program program;
        std::array<std::uint32_t, gpu::Device::kMaxBlocks> blockSizes{};
        std::vector<int> blocks;
        std::vector<int> textures;
        std::vector<std::pair<int, int>> samplers;
    };

    // The uniform blocks and textures of the shader library, which the engine fills itself and materials never list.
    static constexpr std::array<std::string_view, 2> kEngineBlocks{"haylen_vs_params", "haylen_lit_params"};
    static constexpr std::array<std::string_view, 8> kEngineTextures{"sprite_texture", "sprite_texture_1", "sprite_texture_2", "sprite_texture_3", "sprite_texture_4", "sprite_texture_5", "sprite_texture_6", "sprite_texture_7"};

    std::string name;
    std::vector<Shader::Block> blocks;
    std::vector<Shader::Uniform> uniforms;
    std::vector<Shader::TextureSlot> textures;
    std::vector<std::string> sources;
    std::unordered_map<std::string, ProgramInfo> programs;
    std::uint32_t version = 1;
    std::unordered_map<std::string, Compiled> compiled;
    std::unordered_map<std::uint32_t, gpu::Device::Pipeline> pipelines;
    std::weak_ptr<ResourceGraveyard> graveyard;

    ShaderResource() = default;
    ShaderResource(const ShaderResource&) = delete;
    ShaderResource& operator=(const ShaderResource&) = delete;
    ~ShaderResource();

    // Reads and checks a package against itself and the binding limits of every GPU device. Throws `std::invalid_argument` with what is wrong.
    void read(const core::Json& document);

    // Returns the GPU program, creating it on first use. Throws `std::invalid_argument` when the package has no such program or no code of it for the language of the device, and `std::runtime_error` when the device rejects it.
    [[nodiscard]] const Compiled& getProgram(gpu::Device& device, std::string_view program);
    [[nodiscard]] const ProgramInfo& getInfo(std::string_view program) const;

    // Starts compiling the stages of every program for the language of the device on the I/O pool, where the device compiles stages when a program is made, so the first draws wait less.
    void prepare(gpu::Device& device, core::JobSystem& jobs) const;

    // Hands the GPU programs and pipelines to the graveyard, so the next draws create them again.
    void release() noexcept;

  private:
    // The descriptions of a program for a device point into these, which live as long as the call that makes the program.
    struct Described {
        gpu::Device::ProgramDesc desc;
        std::vector<gpu::Device::Attribute> attributes;
        std::vector<gpu::Device::BlockBinding> blocks;
        std::vector<gpu::Device::TextureBinding> textures;
        std::vector<gpu::Device::SamplerBinding> samplers;
        std::vector<gpu::Device::TexturePair> pairs;
    };

    [[nodiscard]] static std::optional<gpu::Device::Language> languageFromName(std::string_view value) noexcept;
    [[nodiscard]] static gpu::Device::Stage readStage(const core::Json& value);
    [[nodiscard]] static int readSlot(const core::Json& value, std::string_view kind, std::size_t limit);
    [[nodiscard]] static gpu::Device::TextureType readTextureType(const core::Json& value);
    [[nodiscard]] static gpu::Device::SampleType readSampleType(const core::Json& value);
    [[nodiscard]] static gpu::Device::SamplerType readSamplerType(const core::Json& value);
    [[nodiscard]] static int findBinding(const std::vector<std::pair<int, int>>& bindings, int slot) noexcept;
    [[nodiscard]] static std::vector<std::pair<int, int>> readBindings(const core::Json& target, const char* key, std::span<const int> slots, std::string_view kind);
    [[nodiscard]] StageCode readStageCode(const core::Json& value) const;
    [[nodiscard]] ProgramInfo readProgram(const std::string& program, const core::Json& value) const;
    void readMaterialBindings(const ProgramInfo& program);

    // Describes a program for a device, or for the language of none, which gives no code.
    void describe(const ProgramInfo& program, gpu::Device::Language language, std::string_view label, Described& described) const;
};

} // namespace haylen::graphics
