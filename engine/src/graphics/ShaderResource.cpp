#include "graphics/ShaderResource.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <utility>

#include "core/Base64.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/core/JsonValidator.hpp"

namespace haylen::graphics {

ShaderResource::~ShaderResource() {
    release();
}

const ShaderResource::Attribute* ShaderResource::ProgramInfo::findAttribute(std::string_view attribute) const noexcept {
    const auto found = std::ranges::find(attributes, attribute, &Attribute::name);
    return found == attributes.end() ? nullptr : &*found;
}

std::optional<gpu::Device::Language> ShaderResource::languageFromName(std::string_view value) noexcept {
    const auto found = std::ranges::find(kLanguageNames, value);
    if (found == kLanguageNames.begin() || found == kLanguageNames.end()) {
        return std::nullopt;
    }
    return static_cast<gpu::Device::Language>(found - kLanguageNames.begin());
}

bool ShaderResource::isBinary(gpu::Device::Language language) noexcept {
    return language == gpu::Device::Language::Spirv || language == gpu::Device::Language::Port;
}

gpu::Device::Stage ShaderResource::readStage(const core::Json& value) {
    const std::string& stage = value.get_ref<const std::string&>();
    if (stage == "vertex") {
        return gpu::Device::Stage::Vertex;
    }
    if (stage == "fragment") {
        return gpu::Device::Stage::Fragment;
    }
    throw std::invalid_argument(std::format("Shaders have no \"{}\" stage.", stage));
}

int ShaderResource::readSlot(const core::Json& value, std::string_view kind, std::size_t limit) {
    const auto slot = value.get<std::int64_t>();
    if (slot < 0 || static_cast<std::uint64_t>(slot) >= limit) {
        throw std::invalid_argument(std::format("The {} slot {} is out of range.", kind, slot));
    }
    return static_cast<int>(slot);
}

gpu::Device::TextureType ShaderResource::readTextureType(const core::Json& value) {
    const std::string& type = value.get_ref<const std::string&>();
    if (type == "2d") {
        return gpu::Device::TextureType::Flat;
    }
    if (type == "2d_array") {
        return gpu::Device::TextureType::Array;
    }
    if (type == "3d") {
        return gpu::Device::TextureType::Volume;
    }
    if (type == "cube") {
        return gpu::Device::TextureType::Cube;
    }
    throw std::invalid_argument(std::format("Shaders have no \"{}\" textures.", type));
}

gpu::Device::SampleType ShaderResource::readSampleType(const core::Json& value) {
    const std::string& type = value.get_ref<const std::string&>();
    if (type == "float") {
        return gpu::Device::SampleType::Float;
    }
    if (type == "unfilterable_float") {
        return gpu::Device::SampleType::UnfilterableFloat;
    }
    if (type == "depth") {
        return gpu::Device::SampleType::Depth;
    }
    if (type == "sint") {
        return gpu::Device::SampleType::SignedInteger;
    }
    if (type == "uint") {
        return gpu::Device::SampleType::UnsignedInteger;
    }
    throw std::invalid_argument(std::format("Shaders have no textures that sample \"{}\".", type));
}

gpu::Device::SamplerType ShaderResource::readSamplerType(const core::Json& value) {
    const std::string& type = value.get_ref<const std::string&>();
    if (type == "filtering") {
        return gpu::Device::SamplerType::Filtering;
    }
    if (type == "nonfiltering") {
        return gpu::Device::SamplerType::NonFiltering;
    }
    if (type == "comparison") {
        return gpu::Device::SamplerType::Comparison;
    }
    throw std::invalid_argument(std::format("Shaders have no \"{}\" samplers.", type));
}

int ShaderResource::readNumber(const core::Json& value, std::string_view what, std::string_view kind, int slot) {
    const auto number = value.get<std::int64_t>();
    if (number < 0 || number > 255) {
        throw std::invalid_argument(std::format("The {} {} of the {} at slot {} is out of range.", what, number, kind, slot));
    }
    return static_cast<int>(number);
}

// Every slot the program declares needs a binding in every language. SPIR-V and the language of a port name the descriptor set and the binding of each slot, WGSL places the uniform blocks in the bind group 0 and the textures and samplers in 1, and the other languages have no sets.
std::vector<ShaderResource::Binding> ShaderResource::readBindings(const core::Json& target, const char* key, std::span<const int> slots, std::string_view kind, gpu::Device::Language language) {
    std::vector<Binding> bindings;
    if (slots.empty()) {
        return bindings;
    }
    const bool images = std::string_view(key) != "blocks";
    const int group = language == gpu::Device::Language::Wgsl && images ? 1 : 0;
    const core::Json& entries = target.at(key);
    for (const int slot : slots) {
        const std::string name = std::to_string(slot);
        if (!entries.contains(name)) {
            throw std::invalid_argument(std::format("The {} at slot {} has no binding number.", kind, slot));
        }
        const core::Json& entry = entries.at(name);
        if (!isBinary(language)) {
            bindings.push_back({.slot = slot, .binding = readNumber(entry, "binding number", kind, slot), .set = group});
            continue;
        }
        core::JsonValidator::requireKnownKeys(entry, {"set", "binding"}, "a binding");
        bindings.push_back({.slot = slot, .binding = readNumber(entry.at("binding"), "binding number", kind, slot), .set = readNumber(entry.at("set"), "descriptor set", kind, slot)});
    }
    return bindings;
}

ShaderResource::StageCode ShaderResource::readStageCode(const core::Json& value) const {
    core::JsonValidator::requireKnownKeys(value, {"source", "entry", "profile"}, "a stage");
    const auto source = value.at("source").get<std::int64_t>();
    if (source < 0 || static_cast<std::uint64_t>(source) >= sources.size()) {
        throw std::invalid_argument(std::format("A stage names the source {}, which the file does not have.", source));
    }
    return {.source = static_cast<std::size_t>(source), .entry = value.at("entry").get<std::string>(), .profile = value.value("profile", std::string())};
}

ShaderResource::ProgramInfo ShaderResource::readProgram(const std::string& program, const core::Json& value) const {
    core::JsonValidator::requireKnownKeys(value, {"attributes", "blocks", "textures", "samplers", "pairs", "outputs", "targets"}, "a program");
    ProgramInfo info;
    for (const core::Json& attribute : value.at("attributes")) {
        info.attributes.push_back({.location = readSlot(attribute.at("location"), "attribute", gpu::Device::kMaxAttributes), .name = attribute.at("name").get<std::string>()});
    }

    std::vector<int> blockSlots;
    for (const core::Json& block : value.at("blocks")) {
        Block read{.name = block.at("name").get<std::string>(), .stage = readStage(block.at("stage")), .slot = readSlot(block.at("slot"), "uniform block", gpu::Device::kMaxBlocks), .size = block.at("size").get<std::uint32_t>()};
        if (read.size == 0 || read.size % 16 != 0) {
            throw std::invalid_argument(std::format("The uniform block \"{}\" holds {} bytes, which is not a multiple of 16.", read.name, read.size));
        }
        if (std::ranges::find(blockSlots, read.slot) != blockSlots.end()) {
            throw std::invalid_argument(std::format("Two uniform blocks of the program \"{}\" share the slot {}.", program, read.slot));
        }
        blockSlots.push_back(read.slot);
        info.blocks.push_back(std::move(read));
    }

    std::vector<int> textureSlots;
    for (const core::Json& texture : value.at("textures")) {
        TextureInfo read{.name = texture.at("name").get<std::string>(), .stage = readStage(texture.at("stage")), .slot = readSlot(texture.at("slot"), "texture", gpu::Device::kMaxTextures), .type = readTextureType(texture.at("type")), .sampleType = readSampleType(texture.at("sampleType"))};
        if (std::ranges::find(textureSlots, read.slot) != textureSlots.end()) {
            throw std::invalid_argument(std::format("Two textures of the program \"{}\" share the slot {}.", program, read.slot));
        }
        textureSlots.push_back(read.slot);
        info.textures.push_back(std::move(read));
    }

    std::vector<int> samplerSlots;
    for (const core::Json& sampler : value.at("samplers")) {
        SamplerInfo read{.name = sampler.at("name").get<std::string>(), .stage = readStage(sampler.at("stage")), .slot = readSlot(sampler.at("slot"), "sampler", gpu::Device::kMaxSamplers), .type = readSamplerType(sampler.at("type"))};
        if (std::ranges::find(samplerSlots, read.slot) != samplerSlots.end()) {
            throw std::invalid_argument(std::format("Two samplers of the program \"{}\" share the slot {}.", program, read.slot));
        }
        samplerSlots.push_back(read.slot);
        info.samplers.push_back(std::move(read));
    }

    for (const core::Json& pair : value.at("pairs")) {
        Pair read{.stage = readStage(pair.at("stage")), .texture = pair.at("texture").get<int>(), .sampler = pair.at("sampler").get<int>(), .name = pair.at("name").get<std::string>()};
        if (std::ranges::find(textureSlots, read.texture) == textureSlots.end() || std::ranges::find(samplerSlots, read.sampler) == samplerSlots.end()) {
            throw std::invalid_argument(std::format("The texture sampler pair \"{}\" names a texture or sampler the program does not declare.", read.name));
        }
        info.pairs.push_back(std::move(read));
    }

    info.outputs = value.at("outputs").get<int>();
    if (info.outputs < 1 || info.outputs > static_cast<int>(gpu::Device::kMaxColorTargets)) {
        throw std::invalid_argument(std::format("The program \"{}\" writes {} colors, and draws write one to {}.", program, info.outputs, gpu::Device::kMaxColorTargets));
    }

    for (const auto& [languageName, target] : value.at("targets").items()) {
        const std::optional<gpu::Device::Language> language = languageFromName(languageName);
        if (!language) {
            throw std::invalid_argument(std::format("The program \"{}\" has code for \"{}\", which is not a shader language of the engine.", program, languageName));
        }
        core::JsonValidator::requireKnownKeys(target, {"vertex", "fragment", "blocks", "textures", "samplers"}, "a target");
        Target read{.vertex = readStageCode(target.at("vertex")), .fragment = readStageCode(target.at("fragment"))};
        const bool glsl = *language == gpu::Device::Language::Glsl430 || *language == gpu::Device::Language::Glsl300es;
        if (glsl) {
            for (const Block& block : info.blocks) {
                const core::Json& flattened = target.at("blocks").at(std::to_string(block.slot));
                const std::string& type = flattened.at("type").get_ref<const std::string&>();
                if (type != "vec4" && type != "ivec4") {
                    throw std::invalid_argument(std::format("The block \"{}\" is read as \"{}\", and OpenGL reads blocks as arrays of \"vec4\" or \"ivec4\".", block.name, type));
                }
                const int count = flattened.at("count").get<int>();
                if (count * 16 != static_cast<int>(block.size)) {
                    throw std::invalid_argument(std::format("The array of the block \"{}\" does not fill its {} bytes.", block.name, block.size));
                }
                read.glslBlocks.push_back({.slot = block.slot, .name = flattened.at("name").get<std::string>(), .integers = type == "ivec4", .count = count});
            }
        } else {
            read.blockBindings = readBindings(target, "blocks", blockSlots, "uniform block", *language);
        }
        if (!glsl) {
            read.textureBindings = readBindings(target, "textures", textureSlots, "texture", *language);
            read.samplerBindings = readBindings(target, "samplers", samplerSlots, "sampler", *language);
        }
        info.targets[static_cast<std::size_t>(*language)] = std::move(read);
    }
    return info;
}

// The stages of the SPIR-V target and of the target of a port name sources that hold the Base64 text of their code, which is decoded once, when the package is read. A SPIR-V module holds whole words of four bytes.
void ShaderResource::readModules(const ProgramInfo& program, gpu::Device::Language language) {
    const std::optional<Target>& target = program.targets[static_cast<std::size_t>(language)];
    if (!target) {
        return;
    }
    const bool spirv = language == gpu::Device::Language::Spirv;
    for (const std::size_t source : {target->vertex.source, target->fragment.source}) {
        if (modules.contains(source)) {
            continue;
        }
        std::optional<std::vector<std::uint8_t>> decoded = core::Base64::decode(sources[source]);
        if (!decoded || decoded->empty() || (spirv && decoded->size() % 4 != 0)) {
            throw std::invalid_argument(std::format("The source {} is not the Base64 text of {}.", source, spirv ? "a SPIR-V module" : "the code of a port"));
        }
        std::vector<std::byte>& module = modules[source];
        module.resize(decoded->size());
        std::ranges::transform(*decoded, module.begin(), [](std::uint8_t value) { return static_cast<std::byte>(value); });
    }
}

// Materials fill the fragment blocks that the shader library does not own, which every program of the package reads with the same slot and size, and bind the textures it does not own.
void ShaderResource::readMaterialBindings(const ProgramInfo& program) {
    for (const Block& block : program.blocks) {
        if (block.stage != gpu::Device::Stage::Fragment || std::ranges::find(kEngineBlocks, block.name) != kEngineBlocks.end()) {
            continue;
        }
        const auto known = std::ranges::find(blocks, block.name, &Shader::Block::name);
        if (known == blocks.end()) {
            throw std::invalid_argument(std::format("The block \"{}\" has no members in the file.", block.name));
        }
        if (known->slot != block.slot || known->size != block.size) {
            throw std::invalid_argument(std::format("A program reads {} bytes at slot {} from the block \"{}\", which holds {} at slot {}.", block.size, block.slot, block.name, known->size, known->slot));
        }
    }
    for (const TextureInfo& texture : program.textures) {
        if (std::ranges::find(kEngineTextures, texture.name) == kEngineTextures.end() && std::ranges::find(textures, texture.name, &Shader::TextureSlot::name) == textures.end()) {
            textures.push_back({.name = texture.name, .slot = texture.slot});
        }
    }
}

void ShaderResource::read(const core::Json& document) {
    core::JsonValidator::requireKnownKeys(document, {"format", "version", "name", "sources", "programs"}, "a shader file");
    if (document.at("format") != "haylen-shader" || document.at("version") != 2) {
        throw std::invalid_argument("It is not a version 2 Haylen shader package. Compile its source again with the Haylen shader compiler.");
    }
    name = document.at("name").get<std::string>();
    sources = document.at("sources").get<std::vector<std::string>>();
    const core::Json& entries = document.at("programs");
    if (!entries.is_object() || entries.empty()) {
        throw std::invalid_argument("It has no programs.");
    }

    // The members of the material blocks come from the program that declares each block first.
    for (const auto& [program, value] : entries.items()) {
        for (const core::Json& block : value.at("blocks")) {
            const std::string& blockName = block.at("name").get_ref<const std::string&>();
            if (readStage(block.at("stage")) != gpu::Device::Stage::Fragment || std::ranges::find(kEngineBlocks, blockName) != kEngineBlocks.end() || std::ranges::find(blocks, blockName, &Shader::Block::name) != blocks.end()) {
                continue;
            }
            blocks.push_back({.name = blockName, .slot = readSlot(block.at("slot"), "uniform block", gpu::Device::kMaxBlocks), .size = block.at("size").get<std::uint32_t>()});
            for (const core::Json& member : block.at("members")) {
                const std::string& typeName = member.at("type").get_ref<const std::string&>();
                const std::optional<Shader::UniformType> type = Shader::uniformTypeFromName(typeName);
                if (!type) {
                    throw std::invalid_argument(std::format("The uniform \"{}\" has the type \"{}\", which materials do not set.", member.at("name").get<std::string>(), typeName));
                }
                Shader::Uniform uniform{.name = member.at("name").get<std::string>(), .type = *type, .count = member.at("count").get<int>(), .offset = member.at("offset").get<std::uint32_t>(), .block = blocks.size() - 1};
                const std::uint64_t element = static_cast<std::uint64_t>(Shader::getComponentCount(uniform.type)) * 4U;
                const std::uint64_t bytes = uniform.count < 1 ? 0U : (static_cast<std::uint64_t>(uniform.count) - 1U) * Shader::getArrayStride(uniform.type) + element;
                if (uniform.count < 1 || uniform.offset + bytes > blocks.back().size) {
                    throw std::invalid_argument(std::format("The uniform \"{}\" does not fit in the block \"{}\".", uniform.name, blockName));
                }
                uniforms.push_back(std::move(uniform));
            }
        }
    }

    for (const auto& [program, value] : entries.items()) {
        ProgramInfo info = readProgram(program, value);
        readMaterialBindings(info);
        readModules(info, gpu::Device::Language::Spirv);
        readModules(info, gpu::Device::Language::Port);
        programs.emplace(program, std::move(info));
    }
}

const ShaderResource::ProgramInfo& ShaderResource::getInfo(std::string_view program) const {
    const auto found = programs.find(std::string(program));
    if (found == programs.end()) {
        throw std::invalid_argument(std::format("The shader \"{}\" has no \"{}\" program.", name, program));
    }
    return found->second;
}

ShaderResource::Binding ShaderResource::findBinding(const std::vector<Binding>& bindings, int slot) noexcept {
    const auto found = std::ranges::find(bindings, slot, &Binding::slot);
    return found == bindings.end() ? Binding{.slot = slot} : *found;
}

gpu::Device::StageCode ShaderResource::describeStage(const StageCode& stage, gpu::Device::Language language) const {
    if (isBinary(language)) {
        return {.bytecode = modules.at(stage.source), .entry = stage.entry};
    }
    return {.source = sources[stage.source], .entry = stage.entry, .profile = stage.profile};
}

void ShaderResource::describe(const ProgramInfo& program, gpu::Device::Language language, std::string_view label, Described& described) const {
    const std::optional<Target>& target = program.targets[static_cast<std::size_t>(language)];
    if (language != gpu::Device::Language::None && !target) {
        throw std::invalid_argument(std::format("The program \"{}\" has no code for the \"{}\" language of this GPU device. Compile its shader again with the Haylen shader compiler.", label, kLanguageNames[static_cast<std::size_t>(language)]));
    }

    for (const Attribute& attribute : program.attributes) {
        described.attributes.push_back({.location = attribute.location, .name = attribute.name});
    }
    for (const Block& block : program.blocks) {
        gpu::Device::BlockBinding binding{.slot = block.slot, .stage = block.stage, .size = block.size};
        if (target) {
            const Binding found = findBinding(target->blockBindings, block.slot);
            binding.binding = found.binding;
            binding.set = found.set;
            if (const auto glsl = std::ranges::find(target->glslBlocks, block.slot, &GlslBlock::slot); glsl != target->glslBlocks.end()) {
                binding.glslName = glsl->name;
                binding.glslIntegers = glsl->integers;
                binding.glslCount = glsl->count;
            }
        }
        described.blocks.push_back(binding);
    }
    for (const TextureInfo& texture : program.textures) {
        const Binding found = target ? findBinding(target->textureBindings, texture.slot) : Binding{};
        described.textures.push_back({.slot = texture.slot, .stage = texture.stage, .type = texture.type, .sampleType = texture.sampleType, .binding = found.binding, .set = found.set});
    }
    for (const SamplerInfo& sampler : program.samplers) {
        const Binding found = target ? findBinding(target->samplerBindings, sampler.slot) : Binding{};
        described.samplers.push_back({.slot = sampler.slot, .stage = sampler.stage, .type = sampler.type, .binding = found.binding, .set = found.set});
    }
    for (const Pair& pair : program.pairs) {
        described.pairs.push_back({.stage = pair.stage, .texture = pair.texture, .sampler = pair.sampler, .glslName = pair.name});
    }

    described.desc.attributes = described.attributes;
    described.desc.blocks = described.blocks;
    described.desc.textures = described.textures;
    described.desc.samplers = described.samplers;
    described.desc.pairs = described.pairs;
    described.desc.label = label;
    if (target) {
        described.desc.vertex = describeStage(target->vertex, language);
        described.desc.fragment = describeStage(target->fragment, language);
    }
}

const ShaderResource::Compiled& ShaderResource::getProgram(gpu::Device& device, std::string_view program) {
    const std::string key(program);
    if (const auto found = compiled.find(key); found != compiled.end()) {
        return found->second;
    }
    const ProgramInfo& info = getInfo(program);
    const std::string label = std::format("{}/{}", name, program);
    Described described;
    describe(info, device.getLanguage(), label, described);

    Compiled made;
    made.program = device.makeProgram(described.desc);
    for (const Block& block : info.blocks) {
        made.blockSizes[static_cast<std::size_t>(block.slot)] = block.size;
        if (block.stage == gpu::Device::Stage::Fragment) {
            made.blocks.push_back(block.slot);
        }
    }
    for (const TextureInfo& texture : info.textures) {
        made.textures.push_back(texture.slot);
    }
    for (const Pair& pair : info.pairs) {
        if (std::ranges::find(made.samplers, pair.sampler, &std::pair<int, int>::first) == made.samplers.end()) {
            made.samplers.emplace_back(pair.sampler, pair.texture);
        }
    }
    return compiled.emplace(key, std::move(made)).first->second;
}

void ShaderResource::prepare(gpu::Device& device, core::JobSystem& jobs) const {
    const gpu::Device::Language language = device.getLanguage();
    for (const auto& [program, info] : programs) {
        if (info.targets[static_cast<std::size_t>(language)]) {
            Described described;
            describe(info, language, program, described);
            device.prepareProgram(described.desc, jobs);
        }
    }
}

void ShaderResource::release() noexcept {
    const std::shared_ptr<ResourceGraveyard> owner = graveyard.lock();
    if (owner) {
        for (const auto& [key, pipeline] : pipelines) {
            owner->bury(pipeline);
        }
        for (const auto& [key, made] : compiled) {
            owner->bury(made.program);
        }
    }
    pipelines.clear();
    compiled.clear();
}

} // namespace haylen::graphics
