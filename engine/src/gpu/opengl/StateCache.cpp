#include "gpu/opengl/StateCache.hpp"

#include <algorithm>

namespace haylen::gpu::opengl {

void StateCache::toggle(Gl& functions, Gl::Enum capability, bool value) {
    if (value) {
        functions.enable(capability);
    } else {
        functions.disable(capability);
    }
}

void StateCache::reset() {
    blends.fill({});
    masks.fill(15);
    gl.disable(Gl::kBlend);
    gl.blendFuncSeparate(Gl::kOne, Gl::kZero, Gl::kOne, Gl::kZero);
    gl.blendEquationSeparate(Gl::kFunctionAdd, Gl::kFunctionAdd);
    gl.colorMask(1, 1, 1, 1);

    depthTest = false;
    depthCompare = Gl::kAlways;
    depthWrite = true;
    gl.disable(Gl::kDepthTest);
    gl.depthFunc(depthCompare);
    gl.depthMask(1);

    stencilTest = false;
    stencilFront = {};
    stencilBack = {};
    stencilReadMask = 0xFF;
    stencilWriteMask = 0xFF;
    stencilReference = 0;
    gl.disable(Gl::kStencilTest);
    gl.stencilFuncSeparate(Gl::kFrontAndBack, Gl::kAlways, 0, stencilReadMask);
    gl.stencilOpSeparate(Gl::kFrontAndBack, Gl::kKeep, Gl::kKeep, Gl::kKeep);
    gl.stencilMaskSeparate(Gl::kFrontAndBack, stencilWriteMask);

    cull = false;
    cullFace = Gl::kBack;
    frontFace = Gl::kCounterClockwise;
    polygonOffset = false;
    polygonOffsetFactor = 0.0F;
    polygonOffsetUnits = 0.0F;
    alphaToCoverage = false;
    scissorTest = false;
    blendColor = math::Color::transparent();
    gl.disable(Gl::kCullFace);
    gl.cullFace(cullFace);
    gl.frontFace(frontFace);
    gl.disable(Gl::kPolygonOffsetFill);
    gl.polygonOffset(0.0F, 0.0F);
    gl.disable(Gl::kSampleAlphaToCoverage);
    gl.disable(Gl::kScissorTest);
    gl.disable(Gl::kDither);
    gl.blendColor(0.0F, 0.0F, 0.0F, 0.0F);
    if (gl.isDesktop()) {
        framebufferSrgb = false;
        gl.disable(Gl::kFramebufferSrgb);
    }

    viewport = {};
    scissor = {};
    gl.viewport(0, 0, 0, 0);
    gl.scissor(0, 0, 0, 0);

    program = 0;
    vertexArray = 0;
    buffers = {};
    drawFramebuffer = 0;
    readFramebuffer = 0;
    gl.useProgram(0);
    gl.bindVertexArray(0);
    for (const Gl::Enum target : {Gl::kArrayBuffer, Gl::kCopyReadBuffer, Gl::kCopyWriteBuffer, Gl::kPixelPackBuffer, Gl::kPixelUnpackBuffer}) {
        gl.bindBuffer(target, 0);
    }
    gl.bindFramebuffer(Gl::kFramebuffer, 0);

    units.fill({});
    for (std::size_t unit = 0; unit < kMaxUnits; ++unit) {
        gl.activeTexture(Gl::kTexture0 + static_cast<Gl::Enum>(unit));
        gl.bindTexture(Gl::kTexture2d, 0);
        gl.bindSampler(static_cast<Gl::Uint>(unit), 0);
    }
    activeUnit = 0;
    gl.activeTexture(Gl::kTexture0);
}

bool StateCache::hasSameOperations(const StencilFace& first, const StencilFace& second) noexcept {
    return first.fail == second.fail && first.depthFail == second.depthFail && first.pass == second.pass;
}

void StateCache::setCapability(bool& current, Gl::Enum capability, bool value) {
    if (current == value) {
        return;
    }
    current = value;
    toggle(gl, capability, value);
}

void StateCache::applyRaster(const Raster& raster) {
    applyBlends(raster);
    applyMasks(raster);
    applyDepth(raster);
    applyStencil(raster);
    applyFaces(raster);
    setCapability(alphaToCoverage, Gl::kSampleAlphaToCoverage, raster.alphaToCoverage);
}

// A pipeline whose targets share one blend sets it with the calls that reach every draw buffer, and one with a blend per target sets each target that differs.
void StateCache::applyBlends(const Raster& raster) {
    if (!raster.independent) {
        const Blend& wanted = raster.blends.front();
        if (std::ranges::all_of(blends, [&](const Blend& current) { return current == wanted; })) {
            return;
        }
        if (std::ranges::any_of(blends, [&](const Blend& current) { return current.enabled != wanted.enabled; })) {
            toggle(gl, Gl::kBlend, wanted.enabled);
        }
        gl.blendFuncSeparate(wanted.sourceColor, wanted.destinationColor, wanted.sourceAlpha, wanted.destinationAlpha);
        gl.blendEquationSeparate(wanted.colorOperation, wanted.alphaOperation);
        blends.fill(wanted);
        return;
    }

    for (std::size_t target = 0; target < kTargets; ++target) {
        const Blend& wanted = raster.blends[target];
        Blend& current = blends[target];
        if (current == wanted) {
            continue;
        }
        const auto index = static_cast<Gl::Uint>(target);
        if (current.enabled != wanted.enabled) {
            if (wanted.enabled) {
                gl.enablei(Gl::kBlend, index);
            } else {
                gl.disablei(Gl::kBlend, index);
            }
        }
        gl.blendFuncSeparatei(index, wanted.sourceColor, wanted.destinationColor, wanted.sourceAlpha, wanted.destinationAlpha);
        gl.blendEquationSeparatei(index, wanted.colorOperation, wanted.alphaOperation);
        current = wanted;
    }
}

void StateCache::applyMasks(const Raster& raster) {
    if (!raster.independent) {
        const std::uint8_t wanted = raster.masks.front();
        if (std::ranges::all_of(masks, [&](std::uint8_t current) { return current == wanted; })) {
            return;
        }
        gl.colorMask((wanted & 1U) != 0, (wanted & 2U) != 0, (wanted & 4U) != 0, (wanted & 8U) != 0);
        masks.fill(wanted);
        return;
    }

    for (std::size_t target = 0; target < kTargets; ++target) {
        const std::uint8_t wanted = raster.masks[target];
        if (masks[target] == wanted) {
            continue;
        }
        gl.colorMaski(static_cast<Gl::Uint>(target), (wanted & 1U) != 0, (wanted & 2U) != 0, (wanted & 4U) != 0, (wanted & 8U) != 0);
        masks[target] = wanted;
    }
}

void StateCache::applyDepth(const Raster& raster) {
    setCapability(depthTest, Gl::kDepthTest, raster.depthTest);
    if (depthCompare != raster.depthCompare) {
        depthCompare = raster.depthCompare;
        gl.depthFunc(depthCompare);
    }
    if (depthWrite != raster.depthWrite) {
        depthWrite = raster.depthWrite;
        gl.depthMask(depthWrite ? 1 : 0);
    }

    setCapability(polygonOffset, Gl::kPolygonOffsetFill, raster.polygonOffset);
    if (polygonOffsetFactor != raster.polygonOffsetFactor || polygonOffsetUnits != raster.polygonOffsetUnits) {
        polygonOffsetFactor = raster.polygonOffsetFactor;
        polygonOffsetUnits = raster.polygonOffsetUnits;
        gl.polygonOffset(polygonOffsetFactor, polygonOffsetUnits);
    }
}

void StateCache::applyStencil(const Raster& raster) {
    setCapability(stencilTest, Gl::kStencilTest, raster.stencilTest);
    if (!raster.stencilTest) {
        return;
    }

    const bool functionsChanged = stencilFront.compare != raster.stencilFront.compare || stencilBack.compare != raster.stencilBack.compare || stencilReadMask != raster.stencilReadMask;
    const bool operationsChanged = !hasSameOperations(stencilFront, raster.stencilFront) || !hasSameOperations(stencilBack, raster.stencilBack);
    if (operationsChanged) {
        gl.stencilOpSeparate(Gl::kFront, raster.stencilFront.fail, raster.stencilFront.depthFail, raster.stencilFront.pass);
        gl.stencilOpSeparate(Gl::kBack, raster.stencilBack.fail, raster.stencilBack.depthFail, raster.stencilBack.pass);
    }
    stencilFront = raster.stencilFront;
    stencilBack = raster.stencilBack;
    stencilReadMask = raster.stencilReadMask;
    if (functionsChanged) {
        sendStencilFunctions();
    }

    if (stencilWriteMask != raster.stencilWriteMask) {
        stencilWriteMask = raster.stencilWriteMask;
        gl.stencilMaskSeparate(Gl::kFrontAndBack, stencilWriteMask);
    }
}

void StateCache::applyFaces(const Raster& raster) {
    setCapability(cull, Gl::kCullFace, raster.cull);
    if (raster.cull && cullFace != raster.cullFace) {
        cullFace = raster.cullFace;
        gl.cullFace(cullFace);
    }
    if (frontFace != raster.frontFace) {
        frontFace = raster.frontFace;
        gl.frontFace(frontFace);
    }
}

void StateCache::sendStencilFunctions() {
    gl.stencilFuncSeparate(Gl::kFront, stencilFront.compare, stencilReference, stencilReadMask);
    gl.stencilFuncSeparate(Gl::kBack, stencilBack.compare, stencilReference, stencilReadMask);
}

void StateCache::applyStencilReference(std::uint8_t value) {
    if (stencilReference == value) {
        return;
    }
    stencilReference = value;
    sendStencilFunctions();
}

void StateCache::applyBlendColor(const math::Color& value) {
    if (blendColor.r == value.r && blendColor.g == value.g && blendColor.b == value.b && blendColor.a == value.a) {
        return;
    }
    blendColor = value;
    gl.blendColor(value.r, value.g, value.b, value.a);
}

void StateCache::prepareClear() {
    if (std::ranges::any_of(masks, [](std::uint8_t mask) { return mask != 15; })) {
        gl.colorMask(1, 1, 1, 1);
        masks.fill(15);
    }
    if (!depthWrite) {
        depthWrite = true;
        gl.depthMask(1);
    }
    if (stencilWriteMask != 0xFF) {
        stencilWriteMask = 0xFF;
        gl.stencilMaskSeparate(Gl::kFrontAndBack, stencilWriteMask);
    }
    setScissorTest(false);
}

void StateCache::setViewport(const Rectangle& value) {
    if (viewport == value) {
        return;
    }
    viewport = value;
    gl.viewport(value.x, value.y, value.width, value.height);
}

void StateCache::setScissor(const Rectangle& value) {
    if (scissor == value) {
        return;
    }
    scissor = value;
    gl.scissor(value.x, value.y, value.width, value.height);
}

void StateCache::setScissorTest(bool value) {
    setCapability(scissorTest, Gl::kScissorTest, value);
}

void StateCache::setFramebufferSrgb(bool value) {
    if (gl.isDesktop()) {
        setCapability(framebufferSrgb, Gl::kFramebufferSrgb, value);
    }
}

void StateCache::useProgram(Gl::Uint value) {
    if (program == value) {
        return;
    }
    program = value;
    gl.useProgram(value);
}

void StateCache::bindVertexArray(Gl::Uint array) {
    if (vertexArray == array) {
        return;
    }
    vertexArray = array;
    gl.bindVertexArray(array);
}

Gl::Uint& StateCache::getBufferBinding(Gl::Enum target) noexcept {
    switch (target) {
    case Gl::kCopyReadBuffer:
        return buffers.copyRead;
    case Gl::kCopyWriteBuffer:
        return buffers.copyWrite;
    case Gl::kPixelPackBuffer:
        return buffers.pixelPack;
    case Gl::kPixelUnpackBuffer:
        return buffers.pixelUnpack;
    default:
        return buffers.array;
    }
}

// The element array binding belongs to the vertex array, which tracks it itself, so only the bindings of the context reach the cache.
void StateCache::bindBuffer(Gl::Enum target, Gl::Uint buffer) {
    Gl::Uint& current = getBufferBinding(target);
    if (current == buffer) {
        return;
    }
    current = buffer;
    gl.bindBuffer(target, buffer);
}

void StateCache::bindDrawFramebuffer(Gl::Uint framebuffer) {
    if (drawFramebuffer == framebuffer) {
        return;
    }
    drawFramebuffer = framebuffer;
    gl.bindFramebuffer(Gl::kDrawFramebuffer, framebuffer);
}

void StateCache::bindReadFramebuffer(Gl::Uint framebuffer) {
    if (readFramebuffer == framebuffer) {
        return;
    }
    readFramebuffer = framebuffer;
    gl.bindFramebuffer(Gl::kReadFramebuffer, framebuffer);
}

void StateCache::bindTexture(std::size_t unit, Gl::Enum target, Gl::Uint texture) {
    Unit& current = units[unit];
    if (current.texture == texture && current.target == target) {
        return;
    }
    if (activeUnit != unit) {
        activeUnit = unit;
        gl.activeTexture(Gl::kTexture0 + static_cast<Gl::Enum>(unit));
    }
    current.target = target;
    current.texture = texture;
    gl.bindTexture(target, texture);
}

void StateCache::bindSampler(std::size_t unit, Gl::Uint sampler) {
    Unit& current = units[unit];
    if (current.sampler == sampler) {
        return;
    }
    current.sampler = sampler;
    gl.bindSampler(static_cast<Gl::Uint>(unit), sampler);
}

void StateCache::forgetBuffer(Gl::Uint buffer) noexcept {
    for (Gl::Uint* binding : {&buffers.array, &buffers.copyRead, &buffers.copyWrite, &buffers.pixelPack, &buffers.pixelUnpack}) {
        if (*binding == buffer) {
            *binding = 0;
        }
    }
}

void StateCache::forgetTexture(Gl::Uint texture) noexcept {
    for (Unit& unit : units) {
        if (unit.texture == texture) {
            unit.texture = 0;
        }
    }
}

void StateCache::forgetSampler(Gl::Uint sampler) noexcept {
    for (Unit& unit : units) {
        if (unit.sampler == sampler) {
            unit.sampler = 0;
        }
    }
}

void StateCache::forgetProgram(Gl::Uint value) noexcept {
    if (program == value) {
        program = 0;
    }
}

void StateCache::forgetVertexArray(Gl::Uint array) noexcept {
    if (vertexArray == array) {
        vertexArray = 0;
    }
}

void StateCache::forgetFramebuffer(Gl::Uint framebuffer) noexcept {
    if (drawFramebuffer == framebuffer) {
        drawFramebuffer = 0;
    }
    if (readFramebuffer == framebuffer) {
        readFramebuffer = 0;
    }
}

} // namespace haylen::gpu::opengl
