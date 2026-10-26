#include "gpu/direct3d/CompileQueue.hpp"

#include <algorithm>
#include <thread>
#include <utility>

namespace haylen::gpu::direct3d {

CompileQueue::CompileQueue(std::size_t threads) : limit(std::max<std::size_t>(threads, 1)) {}

CompileQueue::~CompileQueue() {
    {
        const std::scoped_lock lock(state->mutex);
        state->stopped = true;
        state->jobs.clear();
    }
    state->wake.notify_all();
}

void CompileQueue::post(std::function<void()> job) {
    bool grow = false;
    {
        const std::scoped_lock lock(state->mutex);
        state->jobs.push_back(std::move(job));
        grow = state->idle == 0 && state->threads < limit;
        if (grow) {
            ++state->threads;
        }
    }
    if (!grow) {
        state->wake.notify_one();
        return;
    }
    // clang-format off
    std::thread([shared = state] {
        work(shared);
    }).detach();
    // clang-format on
}

// A job never lets an exception escape, since the compilations hand their failures to the programs that take them.
void CompileQueue::work(const std::shared_ptr<State>& state) {
    std::unique_lock lock(state->mutex);
    for (;;) {
        ++state->idle;
        state->wake.wait(lock, [&state] { return state->stopped || !state->jobs.empty(); });
        --state->idle;
        if (state->stopped) {
            break;
        }

        std::function<void()> job = std::move(state->jobs.front());
        state->jobs.pop_front();
        lock.unlock();
        job();
        job = nullptr;
        lock.lock();
    }
    --state->threads;
}

} // namespace haylen::gpu::direct3d
