#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "gpu/opengl/Gl.hpp"
#include "gpu/opengl/StateCache.hpp"
#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::opengl {

// A linked OpenGL program made from the GLSL of a shader package. Its uniform blocks are arrays of vectors at locations it looks up once, which keep the values the program was given last, so a block that comes again with the same bytes is not sent. Each texture and sampler pair of the package is a sampler of GLSL that reads the texture unit of the index of the pair.
class ShaderProgram final {
  public:
    // A texture unit of the program, with the slots of the bindings that fill it and the target its texture binds to.
    struct Unit {
        std::size_t texture = 0;
        std::size_t sampler = 0;
        Gl::Enum target = Gl::kTexture2d;
    };

    ShaderProgram(Gl& functions, StateCache& cache, const Device::ProgramDesc& desc);
    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;
    ~ShaderProgram();

    // Sends the bytes of the block at the slot when they differ from the ones the program holds. The program must be the current one.
    void applyUniforms(int slot, std::span<const std::byte> data);

    [[nodiscard]] Gl::Uint getName() const noexcept {
        return name;
    }

    [[nodiscard]] std::span<const Unit> getUnits() const noexcept {
        return units;
    }

  private:
    struct Block {
        Gl::Int location = -1;
        Gl::Sizei count = 0;
        bool integers = false;
        std::vector<std::byte> values;
        bool written = false;
    };

    [[nodiscard]] Gl::Uint compile(Gl::Enum stage, std::string_view source, std::string_view label);
    void link(const Device::ProgramDesc& desc, Gl::Uint vertex, Gl::Uint fragment);
    void readBlocks(const Device::ProgramDesc& desc);
    void readUnits(const Device::ProgramDesc& desc);

    Gl& gl;
    StateCache& state;
    Gl::Uint name = 0;
    std::array<std::int8_t, Device::kMaxBlocks> blockIndices{};
    std::vector<Block> blocks;
    std::vector<Unit> units;
};

} // namespace haylen::gpu::opengl
