#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "graphics/ResourceGraveyard.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/graphics/Shader.hpp"
#include "sokol_gfx.h"

namespace haylen::graphics {

// The contents behind a shader handle: its reflection, its sources and the description of every program for every backend, and the GPU programs and pipelines made from them on the frame thread, which go to the device graveyard when the contents change or the last handle goes away.
struct ShaderResource {
    // A GPU program with the bind slots it declares, which draws fill and leave the others empty: its fragment uniform blocks, its textures, and its samplers with the texture each one samples.
    struct Program {
        sg_shader shader{};
        std::vector<int> blocks;
        std::vector<int> views;
        std::vector<std::pair<int, int>> samplers;
    };

    std::string name;
    std::vector<Shader::Block> blocks;
    std::vector<Shader::Uniform> uniforms;
    std::vector<Shader::TextureSlot> textures;
    std::vector<std::string> sources;
    core::Json programs;
    std::uint32_t version = 1;
    std::unordered_map<std::string, Program> compiled;
    std::unordered_map<std::uint32_t, sg_pipeline> pipelines;
    std::weak_ptr<ResourceGraveyard> graveyard;

    ShaderResource() = default;
    ShaderResource(const ShaderResource&) = delete;
    ShaderResource& operator=(const ShaderResource&) = delete;
    ~ShaderResource();

    // Returns the GPU program of the active backend, creating it on first use. Throws std::invalid_argument when the shader has no such program and std::runtime_error when the backend rejects it.
    [[nodiscard]] const Program& getProgram(std::string_view program);

    // Hands the GPU programs and pipelines to the graveyard, so the next draws create them again.
    void release() noexcept;

  private:
    // The dummy backend of the tests compiles nothing, so the desktop GLSL stands in for it.
    [[nodiscard]] static std::string_view slangOf(sg_backend backend);
    [[nodiscard]] static sg_shader_stage toStage(const std::string& name);
    [[nodiscard]] static sg_uniform_type toUniformType(const std::string& name);
    [[nodiscard]] static sg_image_type toImageType(const std::string& name);
    [[nodiscard]] static sg_image_sample_type toSampleType(const std::string& name);
    [[nodiscard]] static sg_sampler_type toSamplerType(const std::string& name);
    [[nodiscard]] static sg_shader_attr_base_type toBaseType(const std::string& name);

    void describeFunction(sg_shader_function& function, const core::Json& stage) const;
    static void describeBindings(sg_shader_desc& desc, const core::Json& backend);
};

} // namespace haylen::graphics
