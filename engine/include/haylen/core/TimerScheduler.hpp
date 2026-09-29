#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "haylen/core/Connection.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/core/ProcessMode.hpp"

namespace haylen::core {

// Frame-driven timers. Callbacks run inside update() on the frame thread. Each timer runs on scaled or unscaled time and has a process mode, so a timer of a pause menu keeps running while the game is paused.
class TimerScheduler final {
  public:
    using Id = std::uint64_t;

    struct Options {
        // Inherit follows the parent mode, and counts as Pausable without one.
        ProcessMode processMode = ProcessMode::Inherit;

        // Returns the mode of the parent that an inheriting timer follows, such as the scene that owns it. Every update asks again, so the timer follows the parent when its mode changes.
        std::function<ProcessMode()> parentMode;

        // Counts real time, ignoring the time scale.
        bool unscaled = false;
    };

    Id after(float delaySeconds, std::function<void()> callback);
    Id after(float delaySeconds, std::function<void()> callback, Options options);
    Id every(float intervalSeconds, std::function<void()> callback, int count = -1);
    Id every(float intervalSeconds, std::function<void()> callback, int count, Options options);

    void cancel(Id id) noexcept;
    void pause(Id id, bool paused) noexcept;
    void clear() noexcept;
    void update(const FrameClock& clock);

    // Returns a connection that cancels the timer, which ties it to a ConnectionScope. Blocking the connection pauses the timer.
    [[nodiscard]] Connection getConnection(Id id);

    [[nodiscard]] bool isActive(Id id) const noexcept;
    [[nodiscard]] std::size_t size() const noexcept {
        return timers.size() + pending.size();
    }

  private:
    struct Timer final : Connection::Link {
        Id id = 0;
        float interval = 0.0F;
        float remaining = 0.0F;
        int repeatsLeft = 0;
        Options options;
        bool paused = false;
        bool cancelled = false;
        bool firedThisUpdate = false;
        std::function<void()> callback;

        void disconnect() override {
            cancelled = true;
        }
        [[nodiscard]] bool isConnected() const noexcept override {
            return !cancelled;
        }
        void setBlocked(bool value) override {
            paused = value;
        }
        [[nodiscard]] bool isBlocked() const noexcept override {
            return paused;
        }
    };

    [[nodiscard]] static ProcessMode resolveMode(const Options& options);

    Id add(float interval, float delay, int count, std::function<void()> callback, Options options);
    [[nodiscard]] Timer* find(Id id) const noexcept;
    [[nodiscard]] Timer* findNextDue() const noexcept;
    void finishUpdate();

    std::vector<std::shared_ptr<Timer>> timers;
    std::vector<std::shared_ptr<Timer>> pending;
    Id nextId = 1;
    bool updating = false;
};

} // namespace haylen::core
