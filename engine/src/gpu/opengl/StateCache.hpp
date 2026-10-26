#pragma once

#include <array>
#include <cstdint>

#include "gpu/opengl/Gl.hpp"
#include "haylen/gpu/Device.hpp"
#include "haylen/math/Color.hpp"

namespace haylen::gpu::opengl {

// The state of the OpenGL context as the device last set it, so a call that would set what the context already holds is never sent. Every call of the device that changes state goes through it.
class StateCache final {
  public:
    static constexpr std::size_t kMaxUnits = Device::kMaxTextures;
    static constexpr std::size_t kTargets = Device::kMaxColorTargets;

    struct Blend {
        bool enabled = false;
        Gl::Enum sourceColor = Gl::kOne;
        Gl::Enum destinationColor = Gl::kZero;
        Gl::Enum colorOperation = Gl::kFunctionAdd;
        Gl::Enum sourceAlpha = Gl::kOne;
        Gl::Enum destinationAlpha = Gl::kZero;
        Gl::Enum alphaOperation = Gl::kFunctionAdd;

        friend bool operator==(const Blend&, const Blend&) = default;
    };

    struct StencilFace {
        Gl::Enum compare = Gl::kAlways;
        Gl::Enum fail = Gl::kKeep;
        Gl::Enum depthFail = Gl::kKeep;
        Gl::Enum pass = Gl::kKeep;

        friend bool operator==(const StencilFace&, const StencilFace&) = default;
    };

    // The fixed function state of a pipeline, which the device works out once when it makes the pipeline. The masks are bits of `Device::ColorMask`.
    struct Raster {
        std::array<Blend, kTargets> blends{};
        std::array<std::uint8_t, kTargets> masks{15, 15, 15, 15};
        bool independent = false;
        bool depthTest = false;
        Gl::Enum depthCompare = Gl::kAlways;
        bool depthWrite = false;
        bool stencilTest = false;
        StencilFace stencilFront;
        StencilFace stencilBack;
        Gl::Uint stencilReadMask = 0xFF;
        Gl::Uint stencilWriteMask = 0xFF;
        bool cull = false;
        Gl::Enum cullFace = Gl::kBack;
        Gl::Enum frontFace = Gl::kCounterClockwise;
        bool polygonOffset = false;
        Gl::Float polygonOffsetFactor = 0.0F;
        Gl::Float polygonOffsetUnits = 0.0F;
        bool alphaToCoverage = false;
    };

    struct Rectangle {
        Gl::Int x = 0;
        Gl::Int y = 0;
        Gl::Sizei width = 0;
        Gl::Sizei height = 0;

        friend bool operator==(const Rectangle&, const Rectangle&) = default;
    };

    explicit StateCache(Gl& functions) noexcept : gl(functions) {}

    // Sends every state the cache tracks, so the context holds what the cache says whatever the platform left in it.
    void reset();

    void applyRaster(const Raster& raster);
    void applyStencilReference(std::uint8_t value);
    void applyBlendColor(const math::Color& value);

    // Opens every color channel and the depth and stencil writes and turns the scissor test off, since they limit what a clear touches.
    void prepareClear();

    void setViewport(const Rectangle& value);
    void setScissor(const Rectangle& value);
    void setScissorTest(bool value);
    void setFramebufferSrgb(bool value);

    void useProgram(Gl::Uint program);
    void bindVertexArray(Gl::Uint array);
    void bindBuffer(Gl::Enum target, Gl::Uint buffer);
    void bindDrawFramebuffer(Gl::Uint framebuffer);
    void bindReadFramebuffer(Gl::Uint framebuffer);
    void bindTexture(std::size_t unit, Gl::Enum target, Gl::Uint texture);
    void bindSampler(std::size_t unit, Gl::Uint sampler);

    // Drops a deleted object from the bindings, since OpenGL may give its name to a new object.
    void forgetBuffer(Gl::Uint buffer) noexcept;
    void forgetTexture(Gl::Uint texture) noexcept;
    void forgetSampler(Gl::Uint sampler) noexcept;
    void forgetProgram(Gl::Uint program) noexcept;
    void forgetVertexArray(Gl::Uint array) noexcept;
    void forgetFramebuffer(Gl::Uint framebuffer) noexcept;

    [[nodiscard]] Gl::Uint getProgram() const noexcept {
        return program;
    }

    [[nodiscard]] Gl::Uint getDrawFramebuffer() const noexcept {
        return drawFramebuffer;
    }

  private:
    struct Unit {
        Gl::Enum target = Gl::kTexture2d;
        Gl::Uint texture = 0;
        Gl::Uint sampler = 0;
    };

    struct BufferBindings {
        Gl::Uint array = 0;
        Gl::Uint copyRead = 0;
        Gl::Uint copyWrite = 0;
        Gl::Uint pixelPack = 0;
        Gl::Uint pixelUnpack = 0;
    };

    static void toggle(Gl& functions, Gl::Enum capability, bool value);
    [[nodiscard]] static bool hasSameOperations(const StencilFace& first, const StencilFace& second) noexcept;
    [[nodiscard]] Gl::Uint& getBufferBinding(Gl::Enum target) noexcept;
    void setCapability(bool& current, Gl::Enum capability, bool value);
    void applyBlends(const Raster& raster);
    void applyMasks(const Raster& raster);
    void applyDepth(const Raster& raster);
    void applyStencil(const Raster& raster);
    void applyFaces(const Raster& raster);
    void sendStencilFunctions();

    Gl& gl;
    std::array<Blend, kTargets> blends{};
    std::array<std::uint8_t, kTargets> masks{15, 15, 15, 15};
    bool depthTest = false;
    Gl::Enum depthCompare = Gl::kLess;
    bool depthWrite = true;
    bool stencilTest = false;
    StencilFace stencilFront;
    StencilFace stencilBack;
    Gl::Uint stencilReadMask = 0xFFFFFFFF;
    Gl::Uint stencilWriteMask = 0xFFFFFFFF;
    std::uint8_t stencilReference = 0;
    bool cull = false;
    Gl::Enum cullFace = Gl::kBack;
    Gl::Enum frontFace = Gl::kCounterClockwise;
    bool polygonOffset = false;
    Gl::Float polygonOffsetFactor = 0.0F;
    Gl::Float polygonOffsetUnits = 0.0F;
    bool alphaToCoverage = false;
    bool scissorTest = false;
    bool framebufferSrgb = false;
    math::Color blendColor = math::Color::transparent();
    Rectangle viewport;
    Rectangle scissor;
    Gl::Uint program = 0;
    Gl::Uint vertexArray = 0;
    BufferBindings buffers;
    Gl::Uint drawFramebuffer = 0;
    Gl::Uint readFramebuffer = 0;
    std::size_t activeUnit = 0;
    std::array<Unit, kMaxUnits> units{};
};

} // namespace haylen::gpu::opengl
