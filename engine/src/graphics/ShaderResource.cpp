#include "graphics/ShaderResource.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <utility>

namespace haylen::graphics {

ShaderResource::~ShaderResource() {
    release();
}

std::string_view ShaderResource::slangOf(sg_backend backend) {
    switch (backend) {
    case SG_BACKEND_GLCORE:
    case SG_BACKEND_DUMMY:
        return "glsl430";
    case SG_BACKEND_GLES3:
        return "glsl300es";
    case SG_BACKEND_D3D11:
        return "hlsl5";
    case SG_BACKEND_METAL_MACOS:
        return "metal_macos";
    case SG_BACKEND_METAL_IOS:
        return "metal_ios";
    case SG_BACKEND_METAL_SIMULATOR:
        return "metal_sim";
    case SG_BACKEND_WGPU:
        return "wgsl";
    case SG_BACKEND_VULKAN:
        break;
    }
    throw std::runtime_error("Custom shaders have no programs for the active graphics backend.");
}

sg_shader_stage ShaderResource::toStage(const std::string& name) {
    return name == "vertex" ? SG_SHADERSTAGE_VERTEX : SG_SHADERSTAGE_FRAGMENT;
}

sg_uniform_type ShaderResource::toUniformType(const std::string& name) {
    const std::optional<Shader::UniformType> type = Shader::uniformTypeFromName(name);
    if (!type) {
        throw std::invalid_argument(std::format("Shaders do not support uniforms of type {}.", name));
    }
    switch (*type) {
    case Shader::UniformType::Float:
        return SG_UNIFORMTYPE_FLOAT;
    case Shader::UniformType::Vec2:
        return SG_UNIFORMTYPE_FLOAT2;
    case Shader::UniformType::Vec3:
        return SG_UNIFORMTYPE_FLOAT3;
    case Shader::UniformType::Vec4:
        return SG_UNIFORMTYPE_FLOAT4;
    case Shader::UniformType::Int:
        return SG_UNIFORMTYPE_INT;
    case Shader::UniformType::IVec2:
        return SG_UNIFORMTYPE_INT2;
    case Shader::UniformType::IVec3:
        return SG_UNIFORMTYPE_INT3;
    case Shader::UniformType::IVec4:
        return SG_UNIFORMTYPE_INT4;
    case Shader::UniformType::Mat4:
        return SG_UNIFORMTYPE_MAT4;
    }
    return SG_UNIFORMTYPE_FLOAT4;
}

sg_image_type ShaderResource::toImageType(const std::string& name) {
    if (name == "cube") {
        return SG_IMAGETYPE_CUBE;
    }
    if (name == "3d") {
        return SG_IMAGETYPE_3D;
    }
    return name == "array" ? SG_IMAGETYPE_ARRAY : SG_IMAGETYPE_2D;
}

sg_image_sample_type ShaderResource::toSampleType(const std::string& name) {
    if (name == "unfilterable_float") {
        return SG_IMAGESAMPLETYPE_UNFILTERABLE_FLOAT;
    }
    if (name == "sint") {
        return SG_IMAGESAMPLETYPE_SINT;
    }
    if (name == "uint") {
        return SG_IMAGESAMPLETYPE_UINT;
    }
    return name == "depth" ? SG_IMAGESAMPLETYPE_DEPTH : SG_IMAGESAMPLETYPE_FLOAT;
}

sg_sampler_type ShaderResource::toSamplerType(const std::string& name) {
    if (name == "nonfiltering") {
        return SG_SAMPLERTYPE_NONFILTERING;
    }
    return name == "comparison" ? SG_SAMPLERTYPE_COMPARISON : SG_SAMPLERTYPE_FILTERING;
}

sg_shader_attr_base_type ShaderResource::toBaseType(const std::string& name) {
    if (name == "sint") {
        return SG_SHADERATTRBASETYPE_SINT;
    }
    return name == "uint" ? SG_SHADERATTRBASETYPE_UINT : SG_SHADERATTRBASETYPE_FLOAT;
}

void ShaderResource::describeFunction(sg_shader_function& function, const core::Json& stage) const {
    function.source = sources.at(stage.at("source").get<std::size_t>()).c_str();
    function.entry = stage.at("entry").get_ref<const std::string&>().c_str();
    if (stage.contains("d3d11_target")) {
        function.d3d11_target = stage.at("d3d11_target").get_ref<const std::string&>().c_str();
    }
}

void ShaderResource::describeBindings(sg_shader_desc& desc, const core::Json& backend) {
    for (const core::Json& attr : backend.at("attrs")) {
        sg_shader_vertex_attr& target = desc.attrs[attr.at("slot").get<std::size_t>()];
        target.base_type = toBaseType(attr.at("base_type").get<std::string>());
        if (attr.contains("glsl_name")) {
            target.glsl_name = attr.at("glsl_name").get_ref<const std::string&>().c_str();
        }
        if (attr.contains("hlsl_sem_name")) {
            target.hlsl_sem_name = attr.at("hlsl_sem_name").get_ref<const std::string&>().c_str();
            target.hlsl_sem_index = attr.at("hlsl_sem_index").get<std::uint8_t>();
        }
    }

    for (const core::Json& block : backend.at("uniform_blocks")) {
        sg_shader_uniform_block& target = desc.uniform_blocks[block.at("slot").get<std::size_t>()];
        target.stage = toStage(block.at("stage").get<std::string>());
        target.size = block.at("size").get<std::uint32_t>();
        target.layout = SG_UNIFORMLAYOUT_STD140;
        target.hlsl_register_b_n = block.value("hlsl_register_b_n", std::uint8_t{0});
        target.msl_buffer_n = block.value("msl_buffer_n", std::uint8_t{0});
        target.wgsl_group0_binding_n = block.value("wgsl_group0_binding_n", std::uint8_t{0});
        // The description points into the reflection of the resource, so the members are read in place and never copied.
        std::size_t member = 0;
        for (const core::Json& uniform : block.at("glsl_uniforms")) {
            target.glsl_uniforms[member].type = toUniformType(uniform.at("type").get<std::string>());
            target.glsl_uniforms[member].array_count = uniform.at("array_count").get<std::uint16_t>();
            target.glsl_uniforms[member].glsl_name = uniform.at("glsl_name").get_ref<const std::string&>().c_str();
            ++member;
        }
    }

    for (const core::Json& view : backend.at("views")) {
        sg_shader_texture_view& target = desc.views[view.at("slot").get<std::size_t>()].texture;
        target.stage = toStage(view.at("stage").get<std::string>());
        target.image_type = toImageType(view.at("image_type").get<std::string>());
        target.sample_type = toSampleType(view.at("sample_type").get<std::string>());
        target.multisampled = view.at("multisampled").get<bool>();
        target.hlsl_register_t_n = view.value("hlsl_register_t_n", std::uint8_t{0});
        target.msl_texture_n = view.value("msl_texture_n", std::uint8_t{0});
        target.wgsl_group1_binding_n = view.value("wgsl_group1_binding_n", std::uint8_t{0});
    }

    for (const core::Json& sampler : backend.at("samplers")) {
        sg_shader_sampler& target = desc.samplers[sampler.at("slot").get<std::size_t>()];
        target.stage = toStage(sampler.at("stage").get<std::string>());
        target.sampler_type = toSamplerType(sampler.at("sampler_type").get<std::string>());
        target.hlsl_register_s_n = sampler.value("hlsl_register_s_n", std::uint8_t{0});
        target.msl_sampler_n = sampler.value("msl_sampler_n", std::uint8_t{0});
        target.wgsl_group1_binding_n = sampler.value("wgsl_group1_binding_n", std::uint8_t{0});
    }

    for (const core::Json& pair : backend.at("texture_sampler_pairs")) {
        sg_shader_texture_sampler_pair& target = desc.texture_sampler_pairs[pair.at("slot").get<std::size_t>()];
        target.stage = toStage(pair.at("stage").get<std::string>());
        target.view_slot = pair.at("view_slot").get<std::uint8_t>();
        target.sampler_slot = pair.at("sampler_slot").get<std::uint8_t>();
        if (pair.contains("glsl_name")) {
            target.glsl_name = pair.at("glsl_name").get_ref<const std::string&>().c_str();
        }
    }
}

const ShaderResource::Program& ShaderResource::getProgram(std::string_view program) {
    const std::string key(program);
    if (const auto found = compiled.find(key); found != compiled.end()) {
        return found->second;
    }
    if (!programs.contains(key)) {
        throw std::invalid_argument(std::format("The shader {} has no {} program.", name, program));
    }

    const core::Json& backend = programs.at(key).at(std::string(slangOf(sg_query_backend())));
    sg_shader_desc desc{};
    describeFunction(desc.vertex_func, backend.at("vertex"));
    describeFunction(desc.fragment_func, backend.at("fragment"));
    describeBindings(desc, backend);
    desc.label = name.c_str();

    Program made;
    made.shader = sg_make_shader(&desc);
    if (sg_query_shader_state(made.shader) != SG_RESOURCESTATE_VALID) {
        sg_destroy_shader(made.shader);
        throw std::runtime_error(std::format("The graphics backend rejected the {} program of the shader {}.", program, name));
    }

    for (const core::Json& block : backend.at("uniform_blocks")) {
        if (block.at("stage") == "fragment") {
            made.blocks.push_back(block.at("slot").get<int>());
        }
    }
    for (const core::Json& view : backend.at("views")) {
        made.views.push_back(view.at("slot").get<int>());
    }
    for (const core::Json& pair : backend.at("texture_sampler_pairs")) {
        const int sampler = pair.at("sampler_slot").get<int>();
        if (std::none_of(made.samplers.begin(), made.samplers.end(), [sampler](const auto& known) { return known.first == sampler; })) {
            made.samplers.emplace_back(sampler, pair.at("view_slot").get<int>());
        }
    }
    return compiled.emplace(key, std::move(made)).first->second;
}

void ShaderResource::release() noexcept {
    const std::shared_ptr<ResourceGraveyard> owner = graveyard.lock();
    if (owner) {
        for (const auto& [key, pipeline] : pipelines) {
            owner->buryPipeline(pipeline);
        }
        for (const auto& [key, made] : compiled) {
            owner->buryShader(made.shader);
        }
    }
    pipelines.clear();
    compiled.clear();
}

} // namespace haylen::graphics
