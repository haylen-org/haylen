#include "2d/graphics/MaterialResource.hpp"

#include <cmath>
#include <cstring>

namespace haylen::graphics2d {

void MaterialResource::pack(std::vector<std::uint8_t>& bytes) const {
    using Type = graphics::Shader::UniformType;
    const std::vector<graphics::Shader::Block>& blocks = shader.getBlocks();
    const std::size_t first = bytes.size();
    for (const graphics::Shader::Block& block : blocks) {
        bytes.resize(bytes.size() + block.size, 0);
    }

    for (const graphics::Shader::Uniform& uniform : shader.getUniforms()) {
        const auto found = values.find(uniform.name);
        if (found == values.end() || found->second.size() != static_cast<std::size_t>(uniform.count * graphics::Shader::getComponentCount(uniform.type))) {
            continue;
        }

        // The blocks lie one after another, and integer uniforms hold 32-bit integers while every other uniform holds 32-bit floats.
        std::size_t start = first;
        for (std::size_t block = 0; block < uniform.block; ++block) {
            start += blocks[block].size;
        }
        std::uint8_t* target = bytes.data() + start + uniform.offset;
        const bool integer = uniform.type == Type::Int || uniform.type == Type::IVec2 || uniform.type == Type::IVec3 || uniform.type == Type::IVec4;
        for (const float value : found->second) {
            if (integer) {
                const auto number = static_cast<std::int32_t>(std::lround(value));
                std::memcpy(target, &number, sizeof(number));
            } else {
                std::memcpy(target, &value, sizeof(value));
            }
            target += sizeof(float);
        }
    }
}

graphics::Texture MaterialResource::getTextureAt(int slot) const {
    for (const graphics::Shader::TextureSlot& texture : shader.getTextures()) {
        if (texture.slot != slot) {
            continue;
        }
        const auto found = textures.find(texture.name);
        return found == textures.end() ? graphics::Texture{} : found->second;
    }
    return {};
}

} // namespace haylen::graphics2d
