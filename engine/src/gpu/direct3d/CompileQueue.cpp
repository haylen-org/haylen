#include "gpu/direct3d/CompileQueue.hpp"

#include <algorithm>
#include <system_error>
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

std::uint64_t CompileQueue::post(std::function<void()> job) {
    std::uint64_t ticket = 0;
    bool grow = false;
    {
        const std::scoped_lock lock(state->mutex);
        ticket = ++state->tickets;
        state->jobs.push_back({.ticket = ticket, .run = std::move(job)});
        grow = state->jobs.size() > state->idle && state->threads < limit;
        if (grow) {
            ++state->threads;
        }
    }
    if (!grow) {
        state->wake.notify_one();
        return ticket;
    }

    // A thread that does not start takes its job back out of the queue, so no job waits for a thread that never comes.
    try {
        // clang-format off
        std::thread([shared = state] {
            work(shared);
        }).detach();
        // clang-format on
    } catch (const std::system_error&) {
        {
            const std::scoped_lock lock(state->mutex);
            --state->threads;
        }
        withdraw(ticket);
        throw;
    }
    return ticket;
}

bool CompileQueue::withdraw(std::uint64_t ticket) {
    const std::scoped_lock lock(state->mutex);
    const auto found = std::ranges::find(state->jobs, ticket, &Job::ticket);
    if (found == state->jobs.end()) {
        return false;
    }
    state->jobs.erase(found);
    return true;
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

        std::function<void()> job = std::move(state->jobs.front().run);
        state->jobs.pop_front();
        lock.unlock();
        job();
        job = nullptr;
        lock.lock();
    }
    --state->threads;
}

} // namespace haylen::gpu::direct3d
