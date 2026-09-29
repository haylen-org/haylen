#pragma once

#include <chrono>
#include <deque>
#include <memory>

#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Reference.hpp"
#include "haylen/plugins/Plugin.hpp"

namespace haylen::plugins {

// Runs long Lua work as coroutines that share a time budget each frame, since the one Lua state cannot run on worker threads. Jobs pause at jobs.checkpoint once the budget is spent and continue on the next frame.
class JobsPlugin final : public Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "jobs";
    }

    void stop(core::Engine& engine) override;
    void update(core::Engine& engine, float deltaSeconds) override;
    void installLua(core::Engine& engine, lua_State* L) override;

    void spawn(lua_State* L, int arguments, lua::Promise promise);
    void setBudget(std::chrono::microseconds value);
    [[nodiscard]] std::chrono::microseconds getBudget() const noexcept {
        return budget;
    }
    [[nodiscard]] std::size_t getRunningCount() const noexcept {
        return jobs.size();
    }

    // Returns whether the job running on L has used up this frame's budget, and throws when L is not a running job.
    [[nodiscard]] bool isOutOfTime(lua_State* L) const;

  private:
    struct Job {
        lua::Reference thread;
        lua_State* state = nullptr;
        lua::Promise promise;
        int arguments = 0;
    };

    [[nodiscard]] bool resume(Job& job);

    std::deque<Job> jobs;
    std::chrono::microseconds budget{4000};
    std::chrono::steady_clock::time_point deadline{};
    lua_State* current = nullptr;
};

} // namespace haylen::plugins
