#include "gpu/opengl/Gl.hpp"

#include <format>
#include <stdexcept>

#if defined(__EMSCRIPTEN__)
#include <webgl/webgl2.h>
#endif

namespace haylen::gpu::opengl {

void Gl::load(GlContext& context, Device::Api flavour) {
    api = flavour;
    require(context, getError, "glGetError");
    require(context, getIntegerv, "glGetIntegerv");
    require(context, getStringValue, "glGetString");
    require(context, getStringi, "glGetStringi");
    readVersion();
    readFeatures();
    loadCore(context);
    loadOptional(context);
}

std::string Gl::getString(Enum name) const {
    const std::uint8_t* value = getStringValue(name);
    return value == nullptr ? std::string() : std::string(reinterpret_cast<const char*>(value));
}

void Gl::throwMissing(std::string_view name) const {
    throw std::runtime_error(std::format("The OpenGL context lacks the function \"{}\", which the OpenGL device needs.", name));
}

bool Gl::hasExtension(std::string_view name) const {
    return extensions.find(std::format(" {} ", name)) != std::string::npos;
}

void Gl::readVersion() {
    getIntegerv(kMajorVersion, &features.majorVersion);
    getIntegerv(kMinorVersion, &features.minorVersion);
    const std::string version = getString(kVersion);
    const bool embedded = version.starts_with("OpenGL ES") || isWeb();
    const int required = isDesktop() ? 43 : 30;
    const int reported = features.majorVersion * 10 + features.minorVersion;

    if (embedded == isDesktop()) {
        throw std::runtime_error(std::format("The context of version \"{}\" is not of the OpenGL flavour the device draws through.", version));
    }
    if (reported < required) {
        throw std::runtime_error(std::format("The context has OpenGL \"{}\", and the OpenGL device needs {} {}.{} or later.", version, isDesktop() ? "OpenGL" : "OpenGL ES", required / 10, required % 10));
    }
}

void Gl::readFeatures() {
    Int count = 0;
    getIntegerv(kNumExtensions, &count);
    extensions = " ";
    for (Int index = 0; index < count; ++index) {
        if (const std::uint8_t* name = getStringi(kExtensions, static_cast<Uint>(index))) {
            extensions.append(reinterpret_cast<const char*>(name)).push_back(' ');
        }
    }

    const int version = features.majorVersion * 10 + features.minorVersion;
    if (isDesktop()) {
        features.bufferStorage = version >= 44 || hasExtension("GL_ARB_buffer_storage");
        suffixes.robustness = version < 45 && !hasExtension("GL_KHR_robustness") ? "ARB" : "";
        features.vertexAttribBinding = true;
        features.drawBuffersIndexed = true;
        features.timerQuery = true;
        features.debug = true;
        features.robustness = version >= 45 || hasExtension("GL_ARB_robustness") || hasExtension("GL_KHR_robustness");
        features.colorBufferFloat = true;
        features.colorBufferHalfFloat = true;
        features.floatBlend = true;
        features.textureFloatLinear = true;
        features.anisotropicFiltering = version >= 46 || hasExtension("GL_ARB_texture_filter_anisotropic") || hasExtension("GL_EXT_texture_filter_anisotropic");
        features.parallelShaderCompile = hasExtension("GL_KHR_parallel_shader_compile") || hasExtension("GL_ARB_parallel_shader_compile");
        suffixes.parallelShaderCompile = hasExtension("GL_KHR_parallel_shader_compile") ? "KHR" : "ARB";
        return;
    }

    features.colorBufferFloat = hasExtension("GL_EXT_color_buffer_float");
    features.colorBufferHalfFloat = features.colorBufferFloat || hasExtension("GL_EXT_color_buffer_half_float");
    features.floatBlend = hasExtension("GL_EXT_float_blend");
    features.textureFloatLinear = hasExtension("GL_OES_texture_float_linear");
    features.anisotropicFiltering = hasExtension("GL_EXT_texture_filter_anisotropic");
    if (isWeb()) {
        features.timerQuery = hasExtension("GL_EXT_disjoint_timer_query_webgl2");
        return;
    }

    features.bufferStorage = hasExtension("GL_EXT_buffer_storage");
    features.vertexAttribBinding = version >= 31;
    features.drawBuffersIndexed = version >= 32 || hasExtension("GL_OES_draw_buffers_indexed") || hasExtension("GL_EXT_draw_buffers_indexed");
    features.timerQuery = hasExtension("GL_EXT_disjoint_timer_query");
    features.debug = version >= 32 || hasExtension("GL_KHR_debug");
    features.robustness = version >= 32 || hasExtension("GL_KHR_robustness") || hasExtension("GL_EXT_robustness");
    features.parallelShaderCompile = hasExtension("GL_KHR_parallel_shader_compile");

    // The loader of Android answers every name of OpenGL ES 3.2 even on older contexts, with functions that do nothing there, so an older context takes the names of the extension it has.
    suffixes.bufferStorage = "EXT";
    suffixes.timerQuery = "EXT";
    suffixes.parallelShaderCompile = "KHR";
    if (version >= 32) {
        return;
    }
    suffixes.drawBuffersIndexed = hasExtension("GL_OES_draw_buffers_indexed") ? "OES" : "EXT";
    suffixes.debug = "KHR";
    suffixes.robustness = hasExtension("GL_KHR_robustness") ? "KHR" : "EXT";
}

void Gl::loadCore(GlContext& context) {
    require(context, enable, "glEnable");
    require(context, disable, "glDisable");
    require(context, viewport, "glViewport");
    require(context, scissor, "glScissor");
    require(context, colorMask, "glColorMask");
    require(context, depthMask, "glDepthMask");
    require(context, depthFunc, "glDepthFunc");
    require(context, stencilFuncSeparate, "glStencilFuncSeparate");
    require(context, stencilOpSeparate, "glStencilOpSeparate");
    require(context, stencilMaskSeparate, "glStencilMaskSeparate");
    require(context, blendFuncSeparate, "glBlendFuncSeparate");
    require(context, blendEquationSeparate, "glBlendEquationSeparate");
    require(context, blendColor, "glBlendColor");
    require(context, cullFace, "glCullFace");
    require(context, frontFace, "glFrontFace");
    require(context, polygonOffset, "glPolygonOffset");
    require(context, pixelStorei, "glPixelStorei");
    require(context, readPixels, "glReadPixels");
    require(context, readBuffer, "glReadBuffer");
    require(context, drawBuffers, "glDrawBuffers");
    require(context, clearBufferfv, "glClearBufferfv");
    require(context, clearBufferiv, "glClearBufferiv");
    require(context, clearBufferfi, "glClearBufferfi");
    require(context, flush, "glFlush");

    require(context, genBuffers, "glGenBuffers");
    require(context, deleteBuffers, "glDeleteBuffers");
    require(context, bindBuffer, "glBindBuffer");
    require(context, bufferData, "glBufferData");
    require(context, bufferSubData, "glBufferSubData");
    require(context, copyBufferSubData, "glCopyBufferSubData");

    require(context, genTextures, "glGenTextures");
    require(context, deleteTextures, "glDeleteTextures");
    require(context, bindTexture, "glBindTexture");
    require(context, activeTexture, "glActiveTexture");
    require(context, texParameteri, "glTexParameteri");
    require(context, texStorage2D, "glTexStorage2D");
    require(context, texStorage3D, "glTexStorage3D");
    require(context, texSubImage2D, "glTexSubImage2D");
    require(context, texSubImage3D, "glTexSubImage3D");

    require(context, genSamplers, "glGenSamplers");
    require(context, deleteSamplers, "glDeleteSamplers");
    require(context, bindSampler, "glBindSampler");
    require(context, samplerParameteri, "glSamplerParameteri");
    require(context, samplerParameterf, "glSamplerParameterf");

    require(context, genRenderbuffers, "glGenRenderbuffers");
    require(context, deleteRenderbuffers, "glDeleteRenderbuffers");
    require(context, bindRenderbuffer, "glBindRenderbuffer");
    require(context, renderbufferStorageMultisample, "glRenderbufferStorageMultisample");
    require(context, getInternalformativ, "glGetInternalformativ");

    require(context, genFramebuffers, "glGenFramebuffers");
    require(context, deleteFramebuffers, "glDeleteFramebuffers");
    require(context, bindFramebuffer, "glBindFramebuffer");
    require(context, framebufferTexture2D, "glFramebufferTexture2D");
    require(context, framebufferTextureLayer, "glFramebufferTextureLayer");
    require(context, framebufferRenderbuffer, "glFramebufferRenderbuffer");
    require(context, checkFramebufferStatus, "glCheckFramebufferStatus");
    require(context, blitFramebuffer, "glBlitFramebuffer");
    require(context, invalidateFramebuffer, "glInvalidateFramebuffer");

    require(context, createShader, "glCreateShader");
    require(context, shaderSource, "glShaderSource");
    require(context, compileShader, "glCompileShader");
    require(context, getShaderiv, "glGetShaderiv");
    require(context, getShaderInfoLog, "glGetShaderInfoLog");
    require(context, deleteShader, "glDeleteShader");
    require(context, createProgram, "glCreateProgram");
    require(context, attachShader, "glAttachShader");
    require(context, detachShader, "glDetachShader");
    require(context, bindAttribLocation, "glBindAttribLocation");
    require(context, linkProgram, "glLinkProgram");
    require(context, getProgramiv, "glGetProgramiv");
    require(context, getProgramInfoLog, "glGetProgramInfoLog");
    require(context, deleteProgram, "glDeleteProgram");
    require(context, useProgram, "glUseProgram");
    require(context, getUniformLocation, "glGetUniformLocation");
    require(context, uniform1i, "glUniform1i");
    require(context, uniform4fv, "glUniform4fv");
    require(context, uniform4iv, "glUniform4iv");

    require(context, genVertexArrays, "glGenVertexArrays");
    require(context, deleteVertexArrays, "glDeleteVertexArrays");
    require(context, bindVertexArray, "glBindVertexArray");
    require(context, enableVertexAttribArray, "glEnableVertexAttribArray");
    require(context, vertexAttribPointer, "glVertexAttribPointer");
    require(context, vertexAttribIPointer, "glVertexAttribIPointer");
    require(context, vertexAttribDivisor, "glVertexAttribDivisor");

    require(context, drawArraysInstanced, "glDrawArraysInstanced");
    require(context, drawElementsInstanced, "glDrawElementsInstanced");

    require(context, fenceSync, "glFenceSync");
    require(context, clientWaitSync, "glClientWaitSync");
    require(context, getSynciv, "glGetSynciv");
    require(context, deleteSync, "glDeleteSync");

    require(context, genQueries, "glGenQueries");
    require(context, deleteQueries, "glDeleteQueries");
    require(context, beginQuery, "glBeginQuery");
    require(context, endQuery, "glEndQuery");
    require(context, getQueryObjectuiv, "glGetQueryObjectuiv");

    // OpenGL ES reads buffers back only by mapping them, and WebGL2 only by copying them out with a function that Emscripten offers in its header of WebGL2 and not through the context.
    if (isWeb()) {
#if defined(__EMSCRIPTEN__)
        getBufferSubData = &glGetBufferSubData;
#endif
    } else {
        require(context, mapBufferRange, "glMapBufferRange");
        require(context, unmapBuffer, "glUnmapBuffer");
    }
    if (isDesktop()) {
        require(context, getBufferSubData, "glGetBufferSubData");
    }
}

void Gl::bindNamed(GlContext& context, void*& address, std::string_view name, std::string_view suffix) {
    address = context.getProcAddress(std::format("{}{}", name, suffix).c_str());
}

// Each optional feature loads its entry points under the names of the core version or the extension that the context has, and drops the feature when one of them is missing.
void Gl::loadOptional(GlContext& context) {
    if (features.bufferStorage) {
        bindSuffixed(context, bufferStorage, "glBufferStorage", suffixes.bufferStorage);
        features.bufferStorage = bufferStorage != nullptr;
    }

    if (features.vertexAttribBinding) {
        bindSuffixed(context, vertexAttribFormat, "glVertexAttribFormat", "");
        bindSuffixed(context, vertexAttribIFormat, "glVertexAttribIFormat", "");
        bindSuffixed(context, vertexAttribBinding, "glVertexAttribBinding", "");
        bindSuffixed(context, vertexBindingDivisor, "glVertexBindingDivisor", "");
        bindSuffixed(context, bindVertexBuffer, "glBindVertexBuffer", "");
        features.vertexAttribBinding = vertexAttribFormat != nullptr && vertexAttribIFormat != nullptr && vertexAttribBinding != nullptr && vertexBindingDivisor != nullptr && bindVertexBuffer != nullptr;
    }

    if (features.drawBuffersIndexed) {
        bindSuffixed(context, enablei, "glEnablei", suffixes.drawBuffersIndexed);
        bindSuffixed(context, disablei, "glDisablei", suffixes.drawBuffersIndexed);
        bindSuffixed(context, blendFuncSeparatei, "glBlendFuncSeparatei", suffixes.drawBuffersIndexed);
        bindSuffixed(context, blendEquationSeparatei, "glBlendEquationSeparatei", suffixes.drawBuffersIndexed);
        bindSuffixed(context, colorMaski, "glColorMaski", suffixes.drawBuffersIndexed);
        features.drawBuffersIndexed = enablei != nullptr && disablei != nullptr && blendFuncSeparatei != nullptr && blendEquationSeparatei != nullptr && colorMaski != nullptr;
    }

    if (features.timerQuery) {
        bindSuffixed(context, getQueryObjectui64v, "glGetQueryObjectui64v", suffixes.timerQuery);
        features.timerQuery = getQueryObjectui64v != nullptr;
    }

    if (features.debug) {
        bindSuffixed(context, debugMessageCallback, "glDebugMessageCallback", suffixes.debug);
        bindSuffixed(context, debugMessageControl, "glDebugMessageControl", suffixes.debug);
        bindSuffixed(context, objectLabel, "glObjectLabel", suffixes.debug);
        bindSuffixed(context, pushDebugGroup, "glPushDebugGroup", suffixes.debug);
        bindSuffixed(context, popDebugGroup, "glPopDebugGroup", suffixes.debug);
        features.debug = debugMessageCallback != nullptr && debugMessageControl != nullptr && objectLabel != nullptr && pushDebugGroup != nullptr && popDebugGroup != nullptr;
    }

    if (features.robustness) {
        bindSuffixed(context, getGraphicsResetStatus, "glGetGraphicsResetStatus", suffixes.robustness);
        features.robustness = getGraphicsResetStatus != nullptr;
    }

    if (features.parallelShaderCompile) {
        bindSuffixed(context, maxShaderCompilerThreads, "glMaxShaderCompilerThreads", suffixes.parallelShaderCompile);
        features.parallelShaderCompile = maxShaderCompilerThreads != nullptr;
    }
}

} // namespace haylen::gpu::opengl
