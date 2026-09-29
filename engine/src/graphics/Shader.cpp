#include "haylen/graphics/Shader.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <utility>

#include "graphics/ShaderResource.hpp"
#include "haylen/core/JsonValidator.hpp"

namespace haylen::graphics {

const std::array<std::string_view, 9> Shader::kUniformTypeNames = {"float", "vec2", "vec3", "vec4", "int", "ivec2", "ivec3", "ivec4", "mat4"};

std::optional<Shader::UniformType> Shader::uniformTypeFromName(std::string_view name) noexcept {
    const auto found = std::find(kUniformTypeNames.begin(), kUniformTypeNames.end(), name);
    return found == kUniformTypeNames.end() ? std::nullopt : std::optional(static_cast<UniformType>(found - kUniformTypeNames.begin()));
}

std::string_view Shader::uniformTypeName(UniformType type) noexcept {
    return kUniformTypeNames[static_cast<std::size_t>(type)];
}

int Shader::getComponentCount(UniformType type) noexcept {
    switch (type) {
    case UniformType::Float:
    case UniformType::Int:
        return 1;
    case UniformType::Vec2:
    case UniformType::IVec2:
        return 2;
    case UniformType::Vec3:
    case UniformType::IVec3:
        return 3;
    case UniformType::Vec4:
    case UniformType::IVec4:
        return 4;
    case UniformType::Mat4:
        return 16;
    }
    return 1;
}

Shader Shader::parse(std::span<const std::uint8_t> bytes) {
    auto resource = std::make_shared<ShaderResource>();
    try {
        const core::Json document = core::Json::parse(bytes.begin(), bytes.end());
        core::JsonValidator::requireKnownKeys(document, {"format", "version", "name", "blocks", "textures", "sources", "programs"}, "a shader file");
        if (document.at("format") != "haylen-shader" || document.at("version") != 1) {
            throw std::invalid_argument("It is not a version 1 Haylen shader.");
        }

        resource->name = document.at("name").get<std::string>();
        for (const core::Json& block : document.at("blocks")) {
            resource->blocks.push_back({.name = block.at("name").get<std::string>(), .slot = block.at("slot").get<int>(), .size = block.at("size").get<std::uint32_t>()});
            for (const core::Json& uniform : block.at("uniforms")) {
                const std::optional<UniformType> type = uniformTypeFromName(uniform.at("type").get<std::string>());
                if (!type) {
                    throw std::invalid_argument(std::format("The uniform {} has an unsupported type.", uniform.at("name").get<std::string>()));
                }
                resource->uniforms.push_back({.name = uniform.at("name").get<std::string>(), .type = *type, .count = uniform.at("count").get<int>(), .offset = uniform.at("offset").get<std::uint32_t>(), .block = resource->blocks.size() - 1});
            }
        }
        for (const core::Json& texture : document.at("textures")) {
            resource->textures.push_back({.name = texture.at("name").get<std::string>(), .slot = texture.at("slot").get<int>()});
        }
        resource->sources = document.at("sources").get<std::vector<std::string>>();
        resource->programs = document.at("programs");
        if (!resource->programs.is_object()) {
            throw std::invalid_argument("Its programs are not an object.");
        }
        resource->validate();
    } catch (const core::Json::exception& error) {
        throw std::invalid_argument(std::format("The shader file is malformed: {}", error.what()));
    } catch (const std::invalid_argument& error) {
        throw std::invalid_argument(std::format("The shader file is malformed: {}", error.what()));
    }
    return Shader(std::move(resource));
}

const ShaderResource& Shader::getChecked() const {
    if (!resource) {
        throw std::logic_error("The shader handle is empty.");
    }
    return *resource;
}

const std::string& Shader::getName() const {
    return getChecked().name;
}

const std::vector<Shader::Block>& Shader::getBlocks() const {
    return getChecked().blocks;
}

const std::vector<Shader::Uniform>& Shader::getUniforms() const {
    return getChecked().uniforms;
}

const std::vector<Shader::TextureSlot>& Shader::getTextures() const {
    return getChecked().textures;
}

const Shader::Uniform* Shader::findUniform(std::string_view name) const {
    const std::vector<Uniform>& uniforms = getChecked().uniforms;
    const auto found = std::find_if(uniforms.begin(), uniforms.end(), [name](const Uniform& uniform) { return uniform.name == name; });
    return found == uniforms.end() ? nullptr : &*found;
}

const Shader::TextureSlot* Shader::findTexture(std::string_view name) const {
    const std::vector<TextureSlot>& textures = getChecked().textures;
    const auto found = std::find_if(textures.begin(), textures.end(), [name](const TextureSlot& texture) { return texture.name == name; });
    return found == textures.end() ? nullptr : &*found;
}

bool Shader::hasProgram(std::string_view name) const {
    return getChecked().programs.contains(std::string(name));
}

std::uint32_t Shader::getVersion() const {
    return getChecked().version;
}

void Shader::replace(const Shader& fresh) {
    const ShaderResource& source = fresh.getChecked();
    if (!resource) {
        throw std::logic_error("The shader handle is empty.");
    }

    ShaderResource& target = *resource;
    target.release();
    target.name = source.name;
    target.blocks = source.blocks;
    target.uniforms = source.uniforms;
    target.textures = source.textures;
    target.sources = source.sources;
    target.programs = source.programs;
    ++target.version;
}

} // namespace haylen::graphics
