#pragma once

#include <cstddef>

#include "gpu/opengl/Gl.hpp"
#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::opengl {

// Turns the enums of the RHI into the values OpenGL takes.
class Conversions final {
  public:
    // How OpenGL stores a pixel format and takes its pixels. OpenGL ES and WebGL2 have no client format for blue first, so the device swaps red and blue of `Bgra8` pixels while it stages them.
    struct Pixel {
        Gl::Enum internalFormat = Gl::kRgba8;
        Gl::Enum format = Gl::kRgba;
        Gl::Enum type = Gl::kUnsignedByte;
        std::size_t bytes = 4;
        bool depth = false;
        bool stencil = false;
        bool floating = false;
        bool halfFloating = false;
        bool srgb = false;
        bool swapsRedBlue = false;
    };

    struct Vertex {
        Gl::Int size = 1;
        Gl::Enum type = Gl::kFloat;
        bool normalized = false;
        bool integer = false;
    };

    Conversions() = delete;

    [[nodiscard]] static Pixel getPixel(Device::PixelFormat format, bool desktop) noexcept;
    [[nodiscard]] static Vertex getVertex(Device::VertexFormat format) noexcept;
    [[nodiscard]] static Gl::Enum toPrimitive(Device::Primitive primitive) noexcept;
    [[nodiscard]] static Gl::Enum toIndexType(Device::IndexType type) noexcept;
    [[nodiscard]] static std::size_t getIndexSize(Device::IndexType type) noexcept;
    [[nodiscard]] static Gl::Enum toBlendFactor(Device::BlendFactor factor) noexcept;
    [[nodiscard]] static Gl::Enum toBlendOperation(Device::BlendOperation operation) noexcept;
    [[nodiscard]] static Gl::Enum toCompare(Device::CompareFunction function) noexcept;
    [[nodiscard]] static Gl::Enum toStencilOperation(Device::StencilOperation operation) noexcept;
    [[nodiscard]] static Gl::Enum toMinification(Device::Filter minification, Device::Filter mipmap) noexcept;
    [[nodiscard]] static Gl::Enum toMagnification(Device::Filter magnification) noexcept;
    [[nodiscard]] static Gl::Enum toWrap(Device::Wrap wrap) noexcept;
    [[nodiscard]] static Gl::Enum toTextureTarget(Device::TextureType type) noexcept;
};

} // namespace haylen::gpu::opengl
