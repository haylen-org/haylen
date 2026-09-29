#include "haylen/core/TimerScheduler.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace haylen::core {

TimerScheduler::Id TimerScheduler::after(float delaySeconds, std::function<void()> callback) {
    return after(delaySeconds, std::move(callback), Options{});
}

TimerScheduler::Id TimerScheduler::after(float delaySeconds, std::function<void()> callback, Options options) {
    return add(0.0F, delaySeconds, 1, std::move(callback), options);
}

TimerScheduler::Id TimerScheduler::every(float intervalSeconds, std::function<void()> callback, int repeatCount) {
    return every(intervalSeconds, std::move(callback), repeatCount, Options{});
}

TimerScheduler::Id TimerScheduler::every(float intervalSeconds, std::function<void()> callback, int repeatCount, Options options) {
    if (repeatCount == 0) {
        throw std::invalid_argument("A repeating timer needs a positive count, or a negative count to repeat until it is cancelled.");
    }
    return add(intervalSeconds, intervalSeconds, repeatCount, std::move(callback), options);
}

TimerScheduler::Id TimerScheduler::add(float interval, float delay, int repeatCount, std::function<void()> callback, Options options) {
    if (!callback) {
        throw std::invalid_argument("A timer needs a callback.");
    }
    auto timer = std::make_shared<Timer>();
    timer->id = nextId++;
    timer->interval = std::max(0.0F, interval);
    timer->remaining = std::max(0.0F, delay);
    timer->repeatsLeft = repeatCount;
    timer->options = options;
    timer->callback = std::move(callback);

    // Timers created from a callback start on the next update so the running iteration stays valid.
    std::vector<std::shared_ptr<Timer>>& destination = updating ? pending : timers;
    destination.push_back(std::move(timer));
    return destination.back()->id;
}

TimerScheduler::Timer* TimerScheduler::find(Id id) const noexcept {
    for (const auto* list : {&timers, &pending}) {
        const auto found = std::find_if(list->begin(), list->end(), [id](const std::shared_ptr<Timer>& timer) { return timer->id == id; });
        if (found != list->end()) {
            return found->get();
        }
    }
    return nullptr;
}

void TimerScheduler::cancel(Id id) noexcept {
    if (Timer* timer = find(id)) {
        timer->cancelled = true;
    }
}

void TimerScheduler::pause(Id id, bool paused) noexcept {
    if (Timer* timer = find(id)) {
        timer->paused = paused;
    }
}

Connection TimerScheduler::getConnection(Id id) {
    for (const auto* list : {&timers, &pending}) {
        for (const std::shared_ptr<Timer>& timer : *list) {
            if (timer->id == id) {
                return Connection(std::weak_ptr<Connection::Link>(timer));
            }
        }
    }
    return {};
}

void TimerScheduler::clear() noexcept {
    for (const std::shared_ptr<Timer>& timer : pending) {
        timer->cancelled = true;
    }
    pending.clear();
    for (const std::shared_ptr<Timer>& timer : timers) {
        timer->cancelled = true;
    }

    // During an update the running iteration still walks the list, so timers are only marked and removed when it ends.
    if (!updating) {
        timers.clear();
    }
}

bool TimerScheduler::isActive(Id id) const noexcept {
    const Timer* timer = find(id);
    return timer != nullptr && !timer->cancelled;
}

TimerScheduler::Timer* TimerScheduler::findNextDue() const noexcept {
    Timer* due = nullptr;
    for (const std::shared_ptr<Timer>& timer : timers) {
        const bool eligible = !timer->cancelled && !timer->paused && !timer->firedThisUpdate && timer->repeatsLeft != 0;
        if (eligible && timer->remaining <= 0.0F && (due == nullptr || timer->remaining < due->remaining)) {
            due = timer.get();
        }
    }
    return due;
}

void TimerScheduler::update(const FrameClock& clock) {
    updating = true;

    // A timer that its process mode holds keeps its remaining time and cannot fire in this update.
    for (const std::shared_ptr<Timer>& timer : timers) {
        const bool running = !timer->cancelled && !timer->paused && clock.canProcess(timer->options.processMode);
        timer->firedThisUpdate = !running;
        if (running) {
            timer->remaining -= static_cast<float>(timer->options.unscaled ? clock.getUnscaledDelta() : clock.getDelta());
        }
    }

    // Due timers fire in the order they expired, so a timer can still cancel another one that expires later in the same update.
    try {
        while (Timer* timer = findNextDue()) {
            if (timer->repeatsLeft > 0) {
                --timer->repeatsLeft;
            }
            timer->remaining += timer->interval;
            timer->firedThisUpdate = timer->interval <= 0.0F;
            timer->cancelled = timer->repeatsLeft == 0;
            timer->callback();
        }
    } catch (...) {
        finishUpdate();
        throw;
    }
    finishUpdate();
}

void TimerScheduler::finishUpdate() {
    updating = false;
    std::erase_if(timers, [](const std::shared_ptr<Timer>& timer) { return timer->cancelled; });
    std::move(pending.begin(), pending.end(), std::back_inserter(timers));
    pending.clear();
}

} // namespace haylen::core
