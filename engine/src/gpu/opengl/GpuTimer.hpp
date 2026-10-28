#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

#include "gpu/opengl/Gl.hpp"

namespace haylen::gpu::opengl {

// Measures the time the GPU takes for the passes of each frame with a `GL_TIME_ELAPSED` query around each pass, from OpenGL, `EXT_disjoint_timer_query` on OpenGL ES or `EXT_disjoint_timer_query_webgl2` on WebGL2, and adds them up. A query ends with its pass, so none spans the flush that ends a pass, which the OpenGL ES driver r19p0 of the Mali-G72 never finishes. It reads the queries of earlier frames only once all their results are there, so it never waits for the GPU, and a frame that finds every earlier frame still running on the GPU goes unmeasured. OpenGL ES and WebGL2 also read whether a disjoint event spoiled the results, except on a driver that refuses that state, and OpenGL has no such state.
class GpuTimer final {
  public:
    static constexpr std::size_t kFrames = 4;

    explicit GpuTimer(Gl& functions);
    GpuTimer(const GpuTimer&) = delete;
    GpuTimer& operator=(const GpuTimer&) = delete;
    ~GpuTimer();

    void beginFrame();
    void beginPass();
    void endPass();
    void endFrame();

    [[nodiscard]] std::optional<double> getSeconds() const noexcept {
        return seconds;
    }

    [[nodiscard]] bool isAvailable() const noexcept {
        return available;
    }

    void setReadsDisjoint(bool value) noexcept {
        readsDisjoint = value;
    }

  private:
    // The queries a frame keeps for its passes, which grow to the most passes a frame drew and are made again by no later frame.
    struct Frame {
        std::vector<Gl::Uint> queries;
        std::size_t used = 0;
        bool pending = false;
    };

    void collect();

    Gl& gl;
    bool available = false;
    bool readsDisjoint = false;
    std::array<Frame, kFrames> frames;
    std::size_t next = 0;
    std::size_t oldest = 0;
    bool measuring = false;
    bool running = false;
    std::optional<double> seconds;
};

} // namespace haylen::gpu::opengl
