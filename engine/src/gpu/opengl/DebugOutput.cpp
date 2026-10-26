#include "gpu/opengl/DebugOutput.hpp"

#include <algorithm>

#include "haylen/core/Log.hpp"

namespace haylen::gpu::opengl {

DebugOutput::DebugOutput(Gl& functions, bool validation) : gl(functions), enabled(validation), messages(validation && functions.getFeatures().debug) {
    if (!messages) {
        return;
    }
    gl.getIntegerv(Gl::kMaxLabelLength, &maxLabelLength);
    gl.enable(Gl::kDebugOutput);
    gl.enable(Gl::kDebugOutputSynchronous);
    gl.debugMessageCallback(&DebugOutput::receive, nullptr);
    gl.debugMessageControl(Gl::kDontCare, Gl::kDontCare, Gl::kDebugSeverityNotification, 0, nullptr, 0);
    gl.debugMessageControl(Gl::kDontCare, Gl::kDebugTypePushGroup, Gl::kDontCare, 0, nullptr, 0);
    gl.debugMessageControl(Gl::kDontCare, Gl::kDebugTypePopGroup, Gl::kDontCare, 0, nullptr, 0);
}

DebugOutput::~DebugOutput() {
    if (messages) {
        gl.debugMessageCallback(nullptr, nullptr);
        gl.disable(Gl::kDebugOutput);
    }
}

// Errors and undefined behavior are failures of the device, and the performance and portability notes of drivers are details for the debug log.
void DebugOutput::receive(Gl::Enum, Gl::Enum type, Gl::Uint, Gl::Enum severity, Gl::Sizei length, const char* message, const void*) {
    const std::string_view text(message, length < 0 ? std::char_traits<char>::length(message) : static_cast<std::size_t>(length));
    const bool failure = type == Gl::kDebugTypeError || type == Gl::kDebugTypeUndefinedBehavior || type == Gl::kDebugTypeDeprecatedBehavior || severity == Gl::kDebugSeverityHigh;
    if (failure) {
        core::Log::error("OpenGL reported an error: {}", text);
    } else {
        core::Log::debug("OpenGL reported: {}", text);
    }
}

void DebugOutput::label(Gl::Enum type, Gl::Uint name, std::string_view text) const {
    if (!messages || text.empty()) {
        return;
    }
    gl.objectLabel(type, name, std::min(static_cast<Gl::Sizei>(text.size()), maxLabelLength - 1), text.data());
}

void DebugOutput::pushGroup(std::string_view name) const {
    if (messages) {
        gl.pushDebugGroup(Gl::kDebugSourceApplication, 0, std::min(static_cast<Gl::Sizei>(name.size()), maxLabelLength - 1), name.data());
    }
}

void DebugOutput::popGroup() const {
    if (messages) {
        gl.popDebugGroup();
    }
}

void DebugOutput::checkErrors(std::string_view step) const {
    if (!enabled || messages) {
        return;
    }
    for (Gl::Enum error = gl.getError(); error != Gl::kNoError; error = gl.getError()) {
        core::Log::error("OpenGL reported the error 0x{:04X} during {}.", error, step);
    }
}

} // namespace haylen::gpu::opengl
