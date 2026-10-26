#include "gpu/opengl/ShaderProgram.hpp"

#include <algorithm>
#include <cstring>
#include <format>
#include <stdexcept>
#include <string>

#include "gpu/opengl/Conversions.hpp"

namespace haylen::gpu::opengl {

ShaderProgram::ShaderProgram(Gl& functions, StateCache& cache, const Device::ProgramDesc& desc) : gl(functions), state(cache) {
    const Gl::Uint vertex = compile(Gl::kVertexShader, desc.vertex.source, desc.label);
    Gl::Uint fragment = 0;
    try {
        fragment = compile(Gl::kFragmentShader, desc.fragment.source, desc.label);
        link(desc, vertex, fragment);
    } catch (...) {
        gl.deleteShader(vertex);
        gl.deleteShader(fragment);
        throw;
    }
    gl.deleteShader(vertex);
    gl.deleteShader(fragment);

    try {
        readBlocks(desc);
        readUnits(desc);
    } catch (...) {
        gl.deleteProgram(name);
        state.forgetProgram(name);
        throw;
    }
}

ShaderProgram::~ShaderProgram() {
    gl.deleteProgram(name);
    state.forgetProgram(name);
}

Gl::Uint ShaderProgram::compile(Gl::Enum stage, std::string_view source, std::string_view label) {
    const Gl::Uint shader = gl.createShader(stage);
    const char* text = source.data();
    const auto length = static_cast<Gl::Int>(source.size());
    gl.shaderSource(shader, 1, &text, &length);
    gl.compileShader(shader);

    Gl::Int compiled = 0;
    gl.getShaderiv(shader, Gl::kCompileStatus, &compiled);
    if (compiled != 0) {
        return shader;
    }
    Gl::Int size = 0;
    gl.getShaderiv(shader, Gl::kInfoLogLength, &size);
    std::string log(static_cast<std::size_t>(std::max(size, 1)), '\0');
    gl.getShaderInfoLog(shader, static_cast<Gl::Sizei>(log.size()), nullptr, log.data());
    gl.deleteShader(shader);
    log.resize(std::strlen(log.c_str()));
    throw std::runtime_error(std::format("The {} stage of the program \"{}\" did not compile: {}", stage == Gl::kVertexShader ? "vertex" : "fragment", label, log));
}

// The attributes take the locations of the package by their names before the program links, since GLSL ES 3.0 places only vertex inputs that name a location.
void ShaderProgram::link(const Device::ProgramDesc& desc, Gl::Uint vertex, Gl::Uint fragment) {
    name = gl.createProgram();
    gl.attachShader(name, vertex);
    gl.attachShader(name, fragment);
    for (const Device::Attribute& attribute : desc.attributes) {
        gl.bindAttribLocation(name, static_cast<Gl::Uint>(attribute.location), std::string(attribute.name).c_str());
    }
    gl.linkProgram(name);
    gl.detachShader(name, vertex);
    gl.detachShader(name, fragment);

    Gl::Int linked = 0;
    gl.getProgramiv(name, Gl::kLinkStatus, &linked);
    if (linked != 0) {
        return;
    }
    Gl::Int size = 0;
    gl.getProgramiv(name, Gl::kInfoLogLength, &size);
    std::string log(static_cast<std::size_t>(std::max(size, 1)), '\0');
    gl.getProgramInfoLog(name, static_cast<Gl::Sizei>(log.size()), nullptr, log.data());
    gl.deleteProgram(name);
    log.resize(std::strlen(log.c_str()));
    throw std::runtime_error(std::format("The program \"{}\" did not link: {}", desc.label, log));
}

void ShaderProgram::readBlocks(const Device::ProgramDesc& desc) {
    blockIndices.fill(-1);
    blocks.reserve(desc.blocks.size());
    for (const Device::BlockBinding& binding : desc.blocks) {
        if (binding.slot < 0 || binding.slot >= static_cast<int>(Device::kMaxBlocks) || binding.glslName.empty() || static_cast<std::uint32_t>(binding.glslCount) * 16U != binding.size) {
            throw std::logic_error(std::format("The program \"{}\" has a uniform block at slot {} that OpenGL cannot read as an array of vectors.", desc.label, binding.slot));
        }
        blockIndices[static_cast<std::size_t>(binding.slot)] = static_cast<std::int8_t>(blocks.size());
        blocks.push_back({.location = gl.getUniformLocation(name, std::string(binding.glslName).c_str()), .count = binding.glslCount, .integers = binding.glslIntegers, .values = std::vector<std::byte>(binding.size)});
    }
}

void ShaderProgram::readUnits(const Device::ProgramDesc& desc) {
    if (desc.pairs.size() > StateCache::kMaxUnits) {
        throw std::logic_error(std::format("The program \"{}\" samples {} textures, and OpenGL programs sample at most {}.", desc.label, desc.pairs.size(), StateCache::kMaxUnits));
    }
    state.useProgram(name);
    units.reserve(desc.pairs.size());
    for (const Device::TexturePair& pair : desc.pairs) {
        const auto texture = std::ranges::find(desc.textures, pair.texture, &Device::TextureBinding::slot);
        if (texture == desc.textures.end() || pair.sampler < 0 || pair.sampler >= static_cast<int>(Device::kMaxSamplers)) {
            throw std::logic_error(std::format("The texture sampler pair \"{}\" of the program \"{}\" names a texture or a sampler the program does not declare.", pair.glslName, desc.label));
        }
        if (texture->multisampled) {
            throw std::logic_error(std::format("The program \"{}\" samples a multisampled texture, which the OpenGL device resolves instead of sampling.", desc.label));
        }
        gl.uniform1i(gl.getUniformLocation(name, std::string(pair.glslName).c_str()), static_cast<Gl::Int>(units.size()));
        units.push_back({.texture = static_cast<std::size_t>(pair.texture), .sampler = static_cast<std::size_t>(pair.sampler), .target = Conversions::toTextureTarget(texture->type)});
    }
}

void ShaderProgram::applyUniforms(int slot, std::span<const std::byte> data) {
    const std::int8_t index = slot >= 0 && slot < static_cast<int>(Device::kMaxBlocks) ? blockIndices[static_cast<std::size_t>(slot)] : std::int8_t{-1};
    if (index < 0) {
        throw std::logic_error(std::format("The program has no uniform block at slot {}.", slot));
    }
    Block& block = blocks[static_cast<std::size_t>(index)];
    if (data.size() != block.values.size()) {
        throw std::logic_error(std::format("The uniform block at slot {} holds {} bytes, not {}.", slot, block.values.size(), data.size()));
    }
    if (block.location < 0 || (block.written && std::memcmp(block.values.data(), data.data(), data.size()) == 0)) {
        return;
    }

    std::memcpy(block.values.data(), data.data(), data.size());
    block.written = true;
    if (block.integers) {
        gl.uniform4iv(block.location, block.count, reinterpret_cast<const Gl::Int*>(block.values.data()));
    } else {
        gl.uniform4fv(block.location, block.count, reinterpret_cast<const Gl::Float*>(block.values.data()));
    }
}

} // namespace haylen::gpu::opengl
