#include "gpu/opengl/GpuTimer.hpp"

namespace haylen::gpu::opengl {

GpuTimer::GpuTimer(Gl& functions) : gl(functions), available(functions.getFeatures().timerQuery) {}

GpuTimer::~GpuTimer() {
    for (const Frame& frame : frames) {
        if (!frame.queries.empty()) {
            gl.deleteQueries(static_cast<Gl::Sizei>(frame.queries.size()), frame.queries.data());
        }
    }
}

void GpuTimer::beginFrame() {
    if (!available) {
        return;
    }
    collect();
    measuring = !frames[next].pending;
    if (measuring) {
        frames[next].used = 0;
    }
}

void GpuTimer::beginPass() {
    if (!measuring) {
        return;
    }
    Frame& frame = frames[next];
    if (frame.used == frame.queries.size()) {
        Gl::Uint query = 0;
        gl.genQueries(1, &query);
        frame.queries.push_back(query);
    }
    gl.beginQuery(Gl::kTimeElapsed, frame.queries[frame.used]);
    running = true;
}

void GpuTimer::endPass() {
    if (!running) {
        return;
    }
    gl.endQuery(Gl::kTimeElapsed);
    running = false;
    ++frames[next].used;
}

void GpuTimer::endFrame() {
    if (!measuring || frames[next].used == 0) {
        return;
    }
    frames[next].pending = true;
    next = (next + 1) % kFrames;
    measuring = false;
}

// A disjoint event, such as a change of the clock of the GPU, makes the results of the queries that ran meanwhile meaningless, so their frame is dropped.
void GpuTimer::collect() {
    while (frames[oldest].pending) {
        Frame& frame = frames[oldest];
        for (std::size_t index = 0; index < frame.used; ++index) {
            Gl::Uint ready = 0;
            gl.getQueryObjectuiv(frame.queries[index], Gl::kQueryResultAvailable, &ready);
            if (ready == 0) {
                return;
            }
        }

        Gl::Uint64 total = 0;
        for (std::size_t index = 0; index < frame.used; ++index) {
            Gl::Uint64 nanoseconds = 0;
            gl.getQueryObjectui64v(frame.queries[index], Gl::kQueryResult, &nanoseconds);
            total += nanoseconds;
        }
        Gl::Int disjoint = 0;
        if (!gl.isDesktop()) {
            gl.getIntegerv(Gl::kGpuDisjoint, &disjoint);
        }
        if (disjoint == 0) {
            seconds = static_cast<double>(total) * 1e-9;
        }
        frame.pending = false;
        oldest = (oldest + 1) % kFrames;
    }
}

} // namespace haylen::gpu::opengl
