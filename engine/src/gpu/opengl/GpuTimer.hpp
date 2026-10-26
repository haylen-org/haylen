#pragma once

#include <array>
#include <cstddef>
#include <optional>

#include "gpu/opengl/Gl.hpp"

namespace haylen::gpu::opengl {

// Measures the time the GPU takes for each frame with a `GL_TIME_ELAPSED` query around the commands of the frame, from OpenGL, `EXT_disjoint_timer_query` on OpenGL ES or `EXT_disjoint_timer_query_webgl2` on WebGL2. It reads the queries of earlier frames only once their results are there, so it never waits for the GPU, and a frame that finds every query still running on the GPU goes unmeasured.
class GpuTimer final {
  public:
    static constexpr std::size_t kQueries = 4;

    explicit GpuTimer(Gl& functions);
    GpuTimer(const GpuTimer&) = delete;
    GpuTimer& operator=(const GpuTimer&) = delete;
    ~GpuTimer();

    void begin();
    void end();

    [[nodiscard]] std::optional<double> getSeconds() const noexcept {
        return seconds;
    }

    [[nodiscard]] bool isAvailable() const noexcept {
        return available;
    }

  private:
    void collect();

    Gl& gl;
    bool available = false;
    std::array<Gl::Uint, kQueries> queries{};
    std::array<bool, kQueries> pending{};
    std::size_t next = 0;
    std::size_t oldest = 0;
    bool running = false;
    std::optional<double> seconds;
};

} // namespace haylen::gpu::opengl
