#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
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

    // Runs the job on a free thread of the queue, which starts another thread while the jobs that wait outnumber the free threads and the queue has room for one more, and returns the ticket that `withdraw` takes. Throws `std::system_error` with the job out of the queue when the system starts no thread for it.
    std::uint64_t post(std::function<void()> job);

    // Takes the job of the ticket back from the queue unless a thread started it, so the caller runs it at once instead of waiting for the jobs queued before it. Returns whether it took it.
    bool withdraw(std::uint64_t ticket);

  private:
    struct Job {
        std::uint64_t ticket = 0;
        std::function<void()> run;
    };

    struct State {
        std::mutex mutex;
        std::condition_variable wake;
        std::deque<Job> jobs;
        std::uint64_t tickets = 0;
        std::size_t threads = 0;
        std::size_t idle = 0;
        bool stopped = false;
    };

    static void work(const std::shared_ptr<State>& state);

    std::shared_ptr<State> state = std::make_shared<State>();
    std::size_t limit;
};

} // namespace haylen::gpu::direct3d
