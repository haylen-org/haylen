#include "graphics/ShaderResource.hpp"

#include <algorithm>
#include <format>
#include <optional>
#include <stdexcept>
#include <utility>

#include "graphics/Gpu.hpp"

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

std::size_t ShaderResource::checkIndex(std::string_view kind, std::int64_t index, std::size_t limit) {
    if (index < 0 || static_cast<std::uint64_t>(index) >= limit) {
        throw std::invalid_argument(std::format("The {} {} is out of range.", kind, index));
    }
    return static_cast<std::size_t>(index);
}

sg_shader_stage ShaderResource::toStage(const std::string& value) {
    if (value == "vertex") {
        return SG_SHADERSTAGE_VERTEX;
    }
    if (value == "fragment") {
        return SG_SHADERSTAGE_FRAGMENT;
    }
    throw std::invalid_argument(std::format("Shaders do not support the \"{}\" stage.", value));
}

sg_uniform_type ShaderResource::toUniformType(Shader::UniformType type) noexcept {
    switch (type) {
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

sg_image_type ShaderResource::toImageType(const std::string& value) {
    if (value == "2d") {
        return SG_IMAGETYPE_2D;
    }
    if (value == "cube") {
        return SG_IMAGETYPE_CUBE;
    }
    if (value == "3d") {
        return SG_IMAGETYPE_3D;
    }
    if (value == "array") {
        return SG_IMAGETYPE_ARRAY;
    }
    throw std::invalid_argument(std::format("Shaders do not support \"{}\" textures.", value));
}

sg_image_sample_type ShaderResource::toSampleType(const std::string& value) {
    if (value == "float") {
        return SG_IMAGESAMPLETYPE_FLOAT;
    }
    if (value == "unfilterable_float") {
        return SG_IMAGESAMPLETYPE_UNFILTERABLE_FLOAT;
    }
    if (value == "sint") {
        return SG_IMAGESAMPLETYPE_SINT;
    }
    if (value == "uint") {
        return SG_IMAGESAMPLETYPE_UINT;
    }
    if (value == "depth") {
        return SG_IMAGESAMPLETYPE_DEPTH;
    }
    throw std::invalid_argument(std::format("Shaders do not support textures that sample \"{}\".", value));
}

sg_sampler_type ShaderResource::toSamplerType(const std::string& value) {
    if (value == "filtering") {
        return SG_SAMPLERTYPE_FILTERING;
    }
    if (value == "nonfiltering") {
        return SG_SAMPLERTYPE_NONFILTERING;
    }
    if (value == "comparison") {
        return SG_SAMPLERTYPE_COMPARISON;
    }
    throw std::invalid_argument(std::format("Shaders do not support \"{}\" samplers.", value));
}

sg_shader_attr_base_type ShaderResource::toBaseType(const std::string& value) {
    if (value == "float") {
        return SG_SHADERATTRBASETYPE_FLOAT;
    }
    if (value == "sint") {
        return SG_SHADERATTRBASETYPE_SINT;
    }
    if (value == "uint") {
        return SG_SHADERATTRBASETYPE_UINT;
    }
    throw std::invalid_argument(std::format("Shaders do not support vertex attributes of type \"{}\".", value));
}

void ShaderResource::describe(sg_shader_desc& desc, const core::Json& backend) const {
    describeFunction(desc.vertex_func, backend.at("vertex"));
    describeFunction(desc.fragment_func, backend.at("fragment"));
    describeBindings(desc, backend);
}

void ShaderResource::describeFunction(sg_shader_function& function, const core::Json& stage) const {
    function.source = sources[checkIndex("source", stage.at("source").get<std::int64_t>(), sources.size())].c_str();
    function.entry = stage.at("entry").get_ref<const std::string&>().c_str();
    if (stage.contains("d3d11_target")) {
        function.d3d11_target = stage.at("d3d11_target").get_ref<const std::string&>().c_str();
    }
}

void ShaderResource::describeBindings(sg_shader_desc& desc, const core::Json& backend) {
    for (const core::Json& attr : backend.at("attrs")) {
        sg_shader_vertex_attr& target = desc.attrs[checkIndex("vertex attribute slot", attr.at("slot").get<std::int64_t>(), SG_MAX_VERTEX_ATTRIBUTES)];
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
        const std::size_t slot = checkIndex("uniform block slot", block.at("slot").get<std::int64_t>(), SG_MAX_UNIFORMBLOCK_BINDSLOTS);
        sg_shader_uniform_block& target = desc.uniform_blocks[slot];
        target.stage = toStage(block.at("stage").get<std::string>());
        target.size = block.at("size").get<std::uint32_t>();
        target.layout = SG_UNIFORMLAYOUT_STD140;
        target.hlsl_register_b_n = block.value("hlsl_register_b_n", std::uint8_t{0});
        target.msl_buffer_n = block.value("msl_buffer_n", std::uint8_t{0});
        target.wgsl_group0_binding_n = block.value("wgsl_group0_binding_n", std::uint8_t{0});

        // The description points into the reflection of the resource, so the members are read in place and never copied.
        // The GL backends read every member at its own offset in the data of the block, so the members must fill the block exactly.
        std::size_t member = 0;
        std::uint64_t end = 0;
        for (const core::Json& uniform : block.at("glsl_uniforms")) {
            end = describeMember(target.glsl_uniforms[checkIndex("uniform block member", static_cast<std::int64_t>(member), SG_MAX_UNIFORMBLOCK_MEMBERS)], uniform, end);
            ++member;
        }
        if (member > 0 && (end + 15U) / 16U * 16U != target.size) {
            throw std::invalid_argument(std::format("The members of the uniform block at slot {} do not fill its {} bytes.", slot, target.size));
        }
    }

    for (const core::Json& view : backend.at("views")) {
        sg_shader_texture_view& target = desc.views[checkIndex("texture slot", view.at("slot").get<std::int64_t>(), SG_MAX_VIEW_BINDSLOTS)].texture;
        target.stage = toStage(view.at("stage").get<std::string>());
        target.image_type = toImageType(view.at("image_type").get<std::string>());
        target.sample_type = toSampleType(view.at("sample_type").get<std::string>());
        target.multisampled = view.at("multisampled").get<bool>();
        target.hlsl_register_t_n = view.value("hlsl_register_t_n", std::uint8_t{0});
        target.msl_texture_n = view.value("msl_texture_n", std::uint8_t{0});
        target.wgsl_group1_binding_n = view.value("wgsl_group1_binding_n", std::uint8_t{0});
    }

    for (const core::Json& sampler : backend.at("samplers")) {
        sg_shader_sampler& target = desc.samplers[checkIndex("sampler slot", sampler.at("slot").get<std::int64_t>(), SG_MAX_SAMPLER_BINDSLOTS)];
        target.stage = toStage(sampler.at("stage").get<std::string>());
        target.sampler_type = toSamplerType(sampler.at("sampler_type").get<std::string>());
        target.hlsl_register_s_n = sampler.value("hlsl_register_s_n", std::uint8_t{0});
        target.msl_sampler_n = sampler.value("msl_sampler_n", std::uint8_t{0});
        target.wgsl_group1_binding_n = sampler.value("wgsl_group1_binding_n", std::uint8_t{0});
    }

    // Draws bind a texture and a sampler for every pair, so a pair only names the ones the program declares.
    for (const core::Json& pair : backend.at("texture_sampler_pairs")) {
        const std::size_t slot = checkIndex("texture sampler pair slot", pair.at("slot").get<std::int64_t>(), SG_MAX_TEXTURE_SAMPLER_PAIRS);
        const std::size_t view = checkIndex("texture slot", pair.at("view_slot").get<std::int64_t>(), SG_MAX_VIEW_BINDSLOTS);
        const std::size_t sampler = checkIndex("sampler slot", pair.at("sampler_slot").get<std::int64_t>(), SG_MAX_SAMPLER_BINDSLOTS);
        if (desc.views[view].texture.stage == SG_SHADERSTAGE_NONE || desc.samplers[sampler].stage == SG_SHADERSTAGE_NONE) {
            throw std::invalid_argument(std::format("The texture sampler pair at slot {} names a texture or sampler the program does not declare.", slot));
        }

        sg_shader_texture_sampler_pair& target = desc.texture_sampler_pairs[slot];
        target.stage = toStage(pair.at("stage").get<std::string>());
        target.view_slot = static_cast<std::uint8_t>(view);
        target.sampler_slot = static_cast<std::uint8_t>(sampler);
        if (pair.contains("glsl_name")) {
            target.glsl_name = pair.at("glsl_name").get_ref<const std::string&>().c_str();
        }
    }
}

std::uint64_t ShaderResource::describeMember(sg_glsl_shader_uniform& member, const core::Json& uniform, std::uint64_t offset) {
    const std::string& typeName = uniform.at("type").get_ref<const std::string&>();
    const std::optional<Shader::UniformType> type = Shader::uniformTypeFromName(typeName);
    if (!type) {
        throw std::invalid_argument(std::format("Shaders do not support uniforms of type \"{}\".", typeName));
    }
    member.type = toUniformType(*type);
    member.array_count = uniform.at("array_count").get<std::uint16_t>();
    member.glsl_name = uniform.at("glsl_name").get_ref<const std::string&>().c_str();

    // A count of 0 marks a member that is not an array, which holds one element like a count of 1. Arrays and members of three or more numbers start on 16 bytes, and every element of an array takes at least 16 bytes.
    const auto bytes = static_cast<std::uint64_t>(Shader::getComponentCount(*type)) * 4U;
    const std::uint64_t count = std::max<std::uint64_t>(member.array_count, 1);
    const std::uint64_t alignment = count > 1 || bytes > 8 ? 16 : bytes;
    const std::uint64_t stride = count > 1 ? std::max<std::uint64_t>(bytes, 16) : bytes;
    return (offset + alignment - 1) / alignment * alignment + stride * count;
}

void ShaderResource::validate() const {
    for (const Shader::Block& block : blocks) {
        checkIndex("uniform block slot", block.slot, SG_MAX_UNIFORMBLOCK_BINDSLOTS);
    }
    for (const Shader::TextureSlot& texture : textures) {
        checkIndex("texture slot", texture.slot, SG_MAX_VIEW_BINDSLOTS);
    }

    // Materials write every uniform at its offset inside the bytes of its block.
    for (const Shader::Uniform& uniform : uniforms) {
        const Shader::Block& block = blocks[uniform.block];
        if (uniform.count < 1) {
            throw std::invalid_argument(std::format("The uniform \"{}\" has no elements.", uniform.name));
        }
        const std::uint64_t bytes = static_cast<std::uint64_t>(uniform.count) * static_cast<std::uint64_t>(Shader::getComponentCount(uniform.type)) * 4U;
        if (uniform.offset + bytes > block.size) {
            throw std::invalid_argument(std::format("The uniform \"{}\" does not fit in the block \"{}\".", uniform.name, block.name));
        }
    }

    // Draws hand each block of the reflection to the programs that declare its slot for the fragment stage, which read exactly their own size.
    for (const auto& [program, languages] : programs.items()) {
        for (const auto& [language, backend] : languages.items()) {
            sg_shader_desc desc{};
            describe(desc, backend);
            for (const Shader::Block& block : blocks) {
                const sg_shader_uniform_block& described = desc.uniform_blocks[static_cast<std::size_t>(block.slot)];
                if (described.stage == SG_SHADERSTAGE_FRAGMENT && described.size != block.size) {
                    throw std::invalid_argument(std::format("The \"{}\" program for \"{}\" reads {} bytes from the block \"{}\", which holds {}.", program, language, described.size, block.name, block.size));
                }
            }
        }
    }
}

std::vector<std::string> ShaderResource::getBackendSources() const {
    const sg_backend backend = sg_query_backend();
    if (backend == SG_BACKEND_VULKAN) {
        return {};
    }
    const std::string slang(slangOf(backend));
    std::vector<std::size_t> used;
    for (const auto& [program, languages] : programs.items()) {
        if (!languages.contains(slang)) {
            continue;
        }
        for (const char* stage : {"vertex", "fragment"}) {
            const auto index = languages.at(slang).at(stage).at("source").get<std::size_t>();
            if (std::ranges::find(used, index) == used.end()) {
                used.push_back(index);
            }
        }
    }
    std::vector<std::string> stages;
    stages.reserve(used.size());
    for (const std::size_t index : used) {
        stages.push_back(sources[index]);
    }
    return stages;
}

const ShaderResource::Program& ShaderResource::getProgram(std::string_view program) {
    const std::string key(program);
    if (const auto found = compiled.find(key); found != compiled.end()) {
        return found->second;
    }
    if (!programs.contains(key)) {
        throw std::invalid_argument(std::format("The shader \"{}\" has no \"{}\" program.", name, program));
    }

    const core::Json& backend = programs.at(key).at(std::string(slangOf(sg_query_backend())));
    sg_shader_desc desc{};
    describe(desc, backend);
    const std::string label = std::format("{}/{}", name, program);
    desc.label = label.c_str();

    Program made;
    made.shader = Gpu::makeShader(desc);
    for (std::size_t slot = 0; slot < made.blockSizes.size(); ++slot) {
        made.blockSizes[slot] = desc.uniform_blocks[slot].size;
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
