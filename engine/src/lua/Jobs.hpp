#pragma once

#include <chrono>
#include <deque>
#include <memory>

#include "haylen/core/Connection.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Reference.hpp"

struct lua_State;

namespace haylen::lua {

// Runs long Lua work as coroutines that share a time budget each frame, since the one Lua state cannot run on worker threads. Jobs pause at `jobs.checkpoint` once the budget is spent and continue on the next frame. The script plugin runs them in its update.
class Jobs final {
  public:
    // Ends every job, which lets go of its coroutine.
    void stop();
    void update();

    // Whether jobs wait for the next frame to go on.
    [[nodiscard]] bool isAnimating() const;

    // Starts the function on top of the stack as a job, which ends with the owner at `owner` when the index is not 0.
    void spawn(lua_State* L, int owner, Promise promise);
    void setBudget(std::chrono::microseconds value);
    [[nodiscard]] std::chrono::microseconds getBudget() const noexcept {
        return budget;
    }
    // The job that runs left the line for its turn, and it counts too.
    [[nodiscard]] std::size_t getRunningCount() const noexcept {
        return jobs.size() + (current != nullptr ? 1 : 0);
    }

    // Returns whether the job running on `L` has used up this frame's budget, and throws when `L` is not a running job.
    [[nodiscard]] bool isOutOfTime(lua_State* L) const;

  private:
    // Ends the job of an owner that ended, before its next turn.
    class Ownership final : public core::Connection::Link {
      public:
        void disconnect() override {
            connected = false;
        }
        [[nodiscard]] bool isConnected() const noexcept override {
            return connected;
        }
        void setBlocked(bool) override {}
        [[nodiscard]] bool isBlocked() const noexcept override {
            return false;
        }

      private:
        bool connected = true;
    };

    struct Job {
        Reference thread;
        lua_State* state = nullptr;
        Promise promise;
        std::shared_ptr<Ownership> ownership;
    };

    [[nodiscard]] bool resume(Job& job);

    std::deque<Job> jobs;
    std::chrono::microseconds budget{4000};
    std::chrono::steady_clock::time_point deadline{};
    lua_State* current = nullptr;
};

} // namespace haylen::lua
