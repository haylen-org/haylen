#include "gpu/opengl/GpuTimer.hpp"

namespace haylen::gpu::opengl {

GpuTimer::GpuTimer(Gl& functions) : gl(functions), available(functions.getFeatures().timerQuery) {
    if (available) {
        gl.genQueries(static_cast<Gl::Sizei>(kQueries), queries.data());
    }
}

GpuTimer::~GpuTimer() {
    if (available) {
        gl.deleteQueries(static_cast<Gl::Sizei>(kQueries), queries.data());
    }
}

void GpuTimer::begin() {
    if (!available) {
        return;
    }
    collect();
    if (pending[next]) {
        return;
    }
    gl.beginQuery(Gl::kTimeElapsed, queries[next]);
    running = true;
}

void GpuTimer::end() {
    if (!running) {
        return;
    }
    gl.endQuery(Gl::kTimeElapsed);
    running = false;
    pending[next] = true;
    next = (next + 1) % kQueries;
}

// A disjoint event, such as a change of the clock of the GPU, makes the results of the queries that ran meanwhile meaningless, so they are dropped.
void GpuTimer::collect() {
    while (pending[oldest]) {
        Gl::Uint ready = 0;
        gl.getQueryObjectuiv(queries[oldest], Gl::kQueryResultAvailable, &ready);
        if (ready == 0) {
            return;
        }
        Gl::Uint64 nanoseconds = 0;
        gl.getQueryObjectui64v(queries[oldest], Gl::kQueryResult, &nanoseconds);
        Gl::Int disjoint = 0;
        if (!gl.isDesktop()) {
            gl.getIntegerv(Gl::kGpuDisjoint, &disjoint);
        }
        if (disjoint == 0) {
            seconds = static_cast<double>(nanoseconds) * 1e-9;
        }
        pending[oldest] = false;
        oldest = (oldest + 1) % kQueries;
    }
}

} // namespace haylen::gpu::opengl
