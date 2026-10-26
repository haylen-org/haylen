#pragma once

#include <string_view>

#include "gpu/opengl/Gl.hpp"

namespace haylen::gpu::opengl {

// The validation of the device. With validation on and `KHR_debug` in the context, the messages of the driver reach the log of the engine as they happen, errors as errors, and objects and debug groups carry names that GPU debuggers show. A context without `KHR_debug`, such as WebGL2, reports its errors through `glGetError`, which the device reads at the end of each pass and frame while validation is on.
class DebugOutput final {
  public:
    DebugOutput(Gl& functions, bool validation);
    DebugOutput(const DebugOutput&) = delete;
    DebugOutput& operator=(const DebugOutput&) = delete;
    ~DebugOutput();

    void label(Gl::Enum type, Gl::Uint name, std::string_view text) const;
    void pushGroup(std::string_view name) const;
    void popGroup() const;

    // Logs the errors OpenGL holds as errors of the step, where the context has no debug output.
    void checkErrors(std::string_view step) const;

    [[nodiscard]] bool isLabeling() const noexcept {
        return messages;
    }

  private:
    static void receive(Gl::Enum source, Gl::Enum type, Gl::Uint id, Gl::Enum severity, Gl::Sizei length, const char* message, const void* user);

    Gl& gl;
    bool enabled = false;
    bool messages = false;
    Gl::Sizei maxLabelLength = 0;
};

} // namespace haylen::gpu::opengl
