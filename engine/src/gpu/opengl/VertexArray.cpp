#include "gpu/opengl/VertexArray.hpp"

#include <cstdint>
#include <stdexcept>

#include "gpu/opengl/Conversions.hpp"

namespace haylen::gpu::opengl {

VertexArray::VertexArray(Gl& functions, StateCache& cache, std::span<const Device::VertexBufferLayout> layouts, std::span<const Device::VertexAttribute> attributes) : gl(functions), state(cache) {
    if (layouts.size() > Device::kMaxVertexBuffers || attributes.size() > Device::kMaxAttributes) {
        throw std::logic_error("A pipeline reads at most four vertex buffers and sixteen attributes.");
    }
    bufferCount = layouts.size();
    for (std::size_t index = 0; index < bufferCount; ++index) {
        strides[index] = layouts[index].stride;
        perInstance[index] = layouts[index].perInstance;
    }
    for (const Device::VertexAttribute& attribute : attributes) {
        if (attribute.buffer < 0 || static_cast<std::size_t>(attribute.buffer) >= bufferCount || attribute.location < 0 || attribute.location >= static_cast<int>(Device::kMaxAttributes)) {
            throw std::logic_error("An attribute of a pipeline reads a vertex buffer it does not lay out or a location past the sixteen a pipeline has.");
        }
        const Conversions::Vertex vertex = Conversions::getVertex(attribute.format);
        inputs[inputCount++] = {.location = static_cast<Gl::Uint>(attribute.location), .buffer = static_cast<std::size_t>(attribute.buffer), .offset = attribute.offset, .size = vertex.size, .type = vertex.type, .normalized = vertex.normalized, .integer = vertex.integer};
    }

    gl.genVertexArrays(1, &name);
    state.bindVertexArray(name);
    if (gl.getFeatures().vertexAttribBinding) {
        describeWithBindings();
    } else {
        describeWithPointers();
    }
}

VertexArray::~VertexArray() {
    gl.deleteVertexArrays(1, &name);
    state.forgetVertexArray(name);
}

void VertexArray::describeWithBindings() {
    for (std::size_t index = 0; index < inputCount; ++index) {
        const Input& input = inputs[index];
        gl.enableVertexAttribArray(input.location);
        if (input.integer) {
            gl.vertexAttribIFormat(input.location, input.size, input.type, static_cast<Gl::Uint>(input.offset));
        } else {
            gl.vertexAttribFormat(input.location, input.size, input.type, input.normalized ? 1 : 0, static_cast<Gl::Uint>(input.offset));
        }
        gl.vertexAttribBinding(input.location, static_cast<Gl::Uint>(input.buffer));
    }
    for (std::size_t buffer = 0; buffer < bufferCount; ++buffer) {
        gl.vertexBindingDivisor(static_cast<Gl::Uint>(buffer), perInstance[buffer] ? 1 : 0);
    }
}

void VertexArray::describeWithPointers() {
    for (std::size_t index = 0; index < inputCount; ++index) {
        const Input& input = inputs[index];
        gl.enableVertexAttribArray(input.location);
        gl.vertexAttribDivisor(input.location, perInstance[input.buffer] ? 1 : 0);
    }
}

void VertexArray::pointInputs(std::size_t buffer, Gl::Uint bufferName, int offset) {
    state.bindBuffer(Gl::kArrayBuffer, bufferName);
    for (std::size_t index = 0; index < inputCount; ++index) {
        const Input& input = inputs[index];
        if (input.buffer != buffer) {
            continue;
        }
        // OpenGL takes the offset of an attribute into the bound buffer in the place of a pointer.
        const auto* start = reinterpret_cast<const void*>(static_cast<std::uintptr_t>(offset + input.offset));
        if (input.integer) {
            gl.vertexAttribIPointer(input.location, input.size, input.type, strides[buffer], start);
        } else {
            gl.vertexAttribPointer(input.location, input.size, input.type, input.normalized ? 1 : 0, strides[buffer], start);
        }
    }
}

void VertexArray::bind(const std::array<Gl::Uint, Device::kMaxVertexBuffers>& buffers, const std::array<int, Device::kMaxVertexBuffers>& offsets, Gl::Uint indexBuffer) {
    state.bindVertexArray(name);
    const bool attribBinding = gl.getFeatures().vertexAttribBinding;
    for (std::size_t buffer = 0; buffer < bufferCount; ++buffer) {
        Binding& current = bound[buffer];
        if (current.buffer == buffers[buffer] && current.offset == offsets[buffer]) {
            continue;
        }
        current = {.buffer = buffers[buffer], .offset = offsets[buffer]};
        if (attribBinding) {
            gl.bindVertexBuffer(static_cast<Gl::Uint>(buffer), current.buffer, current.offset, strides[buffer]);
        } else {
            pointInputs(buffer, current.buffer, current.offset);
        }
    }

    if (boundIndexBuffer != indexBuffer) {
        boundIndexBuffer = indexBuffer;
        gl.bindBuffer(Gl::kElementArrayBuffer, indexBuffer);
    }
}

void VertexArray::forgetBuffer(Gl::Uint buffer) noexcept {
    for (Binding& binding : bound) {
        if (binding.buffer == buffer) {
            binding = {};
        }
    }
    if (boundIndexBuffer == buffer) {
        boundIndexBuffer = 0;
    }
}

} // namespace haylen::gpu::opengl
