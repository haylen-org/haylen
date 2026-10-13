#pragma once

#include <chrono>
#include <deque>
#include <memory>

#include "haylen/core/Connection.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Reference.hpp"
#include "haylen/plugins/Plugin.hpp"

namespace haylen::plugins {

// Runs long Lua work as coroutines that share a time budget each frame, since the one Lua state cannot run on worker threads. Jobs pause at `jobs.checkpoint` once the budget is spent and continue on the next frame.
class JobsPlugin final : public Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "jobs";
    }

    void stop(core::Engine& engine) override;
    void update(core::Engine& engine, float deltaSeconds) override;
    void installLua(core::Engine& engine, lua_State* L) override;

    // Starts the function on top of the stack as a job, which ends with the owner at `owner` when the index is not 0.
    void spawn(lua_State* L, int owner, lua::Promise promise);
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
        lua::Reference thread;
        lua_State* state = nullptr;
        lua::Promise promise;
        std::shared_ptr<Ownership> ownership;
    };

    [[nodiscard]] bool resume(Job& job);

    std::deque<Job> jobs;
    std::chrono::microseconds budget{4000};
    std::chrono::steady_clock::time_point deadline{};
    lua_State* current = nullptr;
};

} // namespace haylen::plugins
