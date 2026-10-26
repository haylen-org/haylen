#pragma once

#include <array>
#include <cstddef>
#include <span>

#include "gpu/opengl/Gl.hpp"
#include "gpu/opengl/StateCache.hpp"
#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::opengl {

// The vertex array object of a pipeline, which holds the layout of its vertex inputs and the buffers they read, and sends only the buffers and offsets that changed since the last draw. Contexts with vertex attribute binding, OpenGL 4.3 and OpenGL ES 3.1, keep the layout in the object and rebind a buffer with one call, and the others point each attribute of a buffer that changed at it again.
class VertexArray final {
  public:
    VertexArray(Gl& functions, StateCache& cache, std::span<const Device::VertexBufferLayout> layouts, std::span<const Device::VertexAttribute> attributes);
    VertexArray(const VertexArray&) = delete;
    VertexArray& operator=(const VertexArray&) = delete;
    ~VertexArray();

    // Binds the object with the vertex buffers at their offsets and the index buffer.
    void bind(const std::array<Gl::Uint, Device::kMaxVertexBuffers>& buffers, const std::array<int, Device::kMaxVertexBuffers>& offsets, Gl::Uint indexBuffer);

    // Forgets a deleted buffer, so the next draw that names a buffer of the same name binds it again.
    void forgetBuffer(Gl::Uint buffer) noexcept;

    [[nodiscard]] Gl::Uint getName() const noexcept {
        return name;
    }

  private:
    struct Input {
        Gl::Uint location = 0;
        std::size_t buffer = 0;
        int offset = 0;
        Gl::Int size = 1;
        Gl::Enum type = Gl::kFloat;
        bool normalized = false;
        bool integer = false;
    };

    struct Binding {
        Gl::Uint buffer = 0;
        int offset = -1;
    };

    void describeWithBindings();
    void describeWithPointers();
    void pointInputs(std::size_t buffer, Gl::Uint bufferName, int offset);

    Gl& gl;
    StateCache& state;
    Gl::Uint name = 0;
    std::array<Input, Device::kMaxAttributes> inputs{};
    std::size_t inputCount = 0;
    std::array<int, Device::kMaxVertexBuffers> strides{};
    std::array<bool, Device::kMaxVertexBuffers> perInstance{};
    std::size_t bufferCount = 0;
    std::array<Binding, Device::kMaxVertexBuffers> bound{};
    Gl::Uint boundIndexBuffer = 0;
};

} // namespace haylen::gpu::opengl
