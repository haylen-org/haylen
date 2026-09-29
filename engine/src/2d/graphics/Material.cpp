#include "haylen/2d/graphics/Material.hpp"

#include <format>
#include <stdexcept>
#include <utility>

#include "2d/graphics/MaterialResource.hpp"

namespace haylen::graphics2d {

const std::array<std::string_view, 6> Material::kPrograms = {"sprite", "sprite_lit", "text", "text_lit", "mesh", "mesh_lit"};

Material::Material(graphics::Shader shader) {
    if (!shader.isValid()) {
        throw std::invalid_argument("A material needs a shader.");
    }
    for (const std::string_view program : kPrograms) {
        if (!shader.hasProgram(program)) {
            throw std::invalid_argument(std::format("The shader {} has no {} program. Compile it with make.py shaders from a source that includes haylen/material.glsl.", shader.getName(), program));
        }
    }
    resource = std::make_shared<MaterialResource>();
    resource->shader = std::move(shader);
}

MaterialResource& Material::getChecked() const {
    if (!resource) {
        throw std::logic_error("The material handle is empty.");
    }
    return *resource;
}

const graphics::Shader& Material::getShader() const {
    return getChecked().shader;
}

const graphics::Shader::Uniform& Material::findUniform(std::string_view name) const {
    const graphics::Shader& shader = getChecked().shader;
    const graphics::Shader::Uniform* uniform = shader.findUniform(name);
    if (uniform == nullptr) {
        throw std::invalid_argument(std::format("The shader {} has no uniform named {}.", shader.getName(), name));
    }
    return *uniform;
}

void Material::set(std::string_view name, float value) {
    const std::array<float, 1> values{value};
    set(name, values);
}

void Material::set(std::string_view name, math::Vec2 value) {
    const std::array<float, 2> values{value.x, value.y};
    set(name, values);
}

void Material::set(std::string_view name, math::Color value) {
    using Type = graphics::Shader::UniformType;
    const Type type = findUniform(name).type;
    if (type == Type::Vec3) {
        const std::array<float, 3> values{value.r, value.g, value.b};
        set(name, values);
        return;
    }
    const std::array<float, 4> values{value.r, value.g, value.b, value.a};
    set(name, values);
}

// The transform moves x to a x + c y + tx and y to b x + d y + ty, and the matrix keeps z and w in place.
void Material::set(std::string_view name, const math::Transform2D& value) {
    const std::array<float, 16> values{value.a, value.b, 0.0F, 0.0F, value.c, value.d, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, value.tx, value.ty, 0.0F, 1.0F};
    set(name, values);
}

void Material::set(std::string_view name, std::span<const float> values) {
    const graphics::Shader::Uniform& uniform = findUniform(name);
    const auto expected = static_cast<std::size_t>(uniform.count * graphics::Shader::getComponentCount(uniform.type));
    if (values.size() != expected) {
        throw std::invalid_argument(std::format("The uniform {} is {} {}, which takes {} numbers, not {}.", name, uniform.count > 1 ? std::format("an array of {}", uniform.count) : std::string("a"), graphics::Shader::uniformTypeName(uniform.type), expected, values.size()));
    }

    MaterialResource& state = getChecked();
    state.values.insert_or_assign(std::string(name), std::vector<float>(values.begin(), values.end()));
    ++state.revision;
}

void Material::setTexture(std::string_view name, graphics::Texture texture) {
    const graphics::Shader& shader = getChecked().shader;
    if (shader.findTexture(name) == nullptr) {
        throw std::invalid_argument(std::format("The shader {} has no texture named {}.", shader.getName(), name));
    }

    MaterialResource& state = getChecked();
    state.textures.insert_or_assign(std::string(name), std::move(texture));
    ++state.revision;
}

std::vector<float> Material::get(std::string_view name) const {
    const graphics::Shader::Uniform& uniform = findUniform(name);
    const MaterialResource& state = getChecked();
    const auto expected = static_cast<std::size_t>(uniform.count * graphics::Shader::getComponentCount(uniform.type));
    const auto found = state.values.find(name);
    return found != state.values.end() && found->second.size() == expected ? found->second : std::vector<float>(expected, 0.0F);
}

graphics::Texture Material::getTexture(std::string_view name) const {
    const MaterialResource& state = getChecked();
    if (state.shader.findTexture(name) == nullptr) {
        throw std::invalid_argument(std::format("The shader {} has no texture named {}.", state.shader.getName(), name));
    }
    const auto found = state.textures.find(name);
    return found == state.textures.end() ? graphics::Texture{} : found->second;
}

} // namespace haylen::graphics2d
