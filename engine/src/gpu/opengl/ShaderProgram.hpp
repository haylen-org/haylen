#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "gpu/opengl/Gl.hpp"
#include "gpu/opengl/StateCache.hpp"
#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::opengl {

// A linked OpenGL program made from the GLSL of a shader package. Each uniform block of the package is a uniform block of GLSL in the layout std140, which reads the range of a buffer bound to the binding point of its slot. Each texture and sampler pair of the package is a sampler of GLSL that reads the texture unit of the index of the pair.
class ShaderProgram final {
  public:
    // A uniform block of the program at its slot, with its size in bytes. A block that the linker removed reads nothing, so nothing is bound for it.
    struct Block {
        int slot = 0;
        std::uint32_t size = 0;
        bool active = false;
    };

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

    // The block at the slot, or nothing when the program has no block there.
    [[nodiscard]] const Block* findBlock(int slot) const noexcept;

    [[nodiscard]] std::span<const Block> getBlocks() const noexcept {
        return blocks;
    }

    [[nodiscard]] Gl::Uint getName() const noexcept {
        return name;
    }

    [[nodiscard]] std::span<const Unit> getUnits() const noexcept {
        return units;
    }

  private:
    [[nodiscard]] Gl::Uint compile(Gl::Enum stage, std::string_view source, std::string_view label);
    void link(const Device::ProgramDesc& desc, Gl::Uint vertex, Gl::Uint fragment);
    void readBlocks(const Device::ProgramDesc& desc);
    void readUnits(const Device::ProgramDesc& desc);

    Gl& gl;
    StateCache& state;
    Gl::Uint name = 0;
    std::vector<Block> blocks;
    std::vector<Unit> units;
};

} // namespace haylen::gpu::opengl
