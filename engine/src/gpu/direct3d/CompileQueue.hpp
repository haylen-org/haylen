#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>

namespace haylen::gpu::direct3d {

// Runs the compilations of programs prepared ahead on threads of its own, which nothing waits for. The compiler of Direct3D cannot stop a compilation, and one with long loops runs for many seconds, so the pools of the engine, which a quit or a restart waits for, never run them. A queue that goes away drops the compilations that did not start, and the ones that run finish on their threads with what they hold.
class CompileQueue final {
  public:
    explicit CompileQueue(std::size_t threads);
    CompileQueue(const CompileQueue&) = delete;
    CompileQueue& operator=(const CompileQueue&) = delete;
    ~CompileQueue();

    // Runs the job on a free thread of the queue, which starts another thread while every thread is busy and the queue has room for one more.
    void post(std::function<void()> job);

  private:
    struct State {
        std::mutex mutex;
        std::condition_variable wake;
        std::deque<std::function<void()>> jobs;
        std::size_t threads = 0;
        std::size_t idle = 0;
        bool stopped = false;
    };

    static void work(const std::shared_ptr<State>& state);

    std::shared_ptr<State> state = std::make_shared<State>();
    std::size_t limit;
};

} // namespace haylen::gpu::direct3d
