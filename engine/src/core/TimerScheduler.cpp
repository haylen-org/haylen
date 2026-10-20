#include "haylen/core/TimerScheduler.hpp"

#include <algorithm>
#include <functional>
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

TimerScheduler::Id TimerScheduler::every(float intervalSeconds, std::function<void()> callback, int count) {
    return every(intervalSeconds, std::move(callback), count, Options{});
}

TimerScheduler::Id TimerScheduler::every(float intervalSeconds, std::function<void()> callback, int count, Options options) {
    if (count == 0) {
        throw std::invalid_argument("A repeating timer needs a positive count, or a negative count to repeat until it is cancelled.");
    }
    return add(intervalSeconds, intervalSeconds, count, std::move(callback), options);
}

ProcessMode TimerScheduler::resolveMode(const Options& options) {
    return options.processMode == ProcessMode::Inherit && options.parentMode ? options.parentMode() : options.processMode;
}

TimerScheduler::Id TimerScheduler::add(float interval, float delay, int count, std::function<void()> callback, Options options) {
    if (!callback) {
        throw std::invalid_argument("A timer needs a callback.");
    }
    auto timer = std::make_shared<Timer>();
    timer->id = nextId++;
    timer->interval = std::max(0.0F, interval);
    timer->remaining = std::max(0.0F, delay);
    timer->repeatsLeft = count;
    timer->options = std::move(options);
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

void TimerScheduler::update(const FrameClock& clock) {
    updating = true;
    due.clear();

    // A timer that its process mode holds keeps its remaining time and cannot fire in this update.
    for (std::size_t index = 0; index < timers.size(); ++index) {
        Timer& timer = *timers[index];
        const bool running = !timer.cancelled && !timer.paused && clock.canProcess(resolveMode(timer.options));
        timer.firedThisUpdate = !running;
        timer.callsThisUpdate = 0;
        if (!running) {
            continue;
        }
        timer.remaining -= static_cast<float>(timer.options.unscaled ? clock.getUnscaledDelta() : clock.getDelta());
        if (timer.remaining <= 0.0F && timer.repeatsLeft != 0) {
            due.emplace_back(timer.remaining, index);
        }
    }

    // Due timers fire in the order they expired, the earlier one in the list first on a tie, so a timer can still cancel another one that expires later in the same update. A repeating timer that fell behind fires again while it is still due, and the timers that the callbacks create wait in the pending list for the next update.
    std::ranges::make_heap(due, std::greater<>());
    try {
        while (!due.empty()) {
            std::ranges::pop_heap(due, std::greater<>());
            const std::size_t index = due.back().second;
            due.pop_back();
            Timer& timer = *timers[index];
            if (timer.cancelled || timer.paused || timer.firedThisUpdate || timer.repeatsLeft == 0) {
                continue;
            }
            if (timer.repeatsLeft > 0) {
                --timer.repeatsLeft;
            }
            timer.remaining += timer.interval;
            // A timer that owes more calls than one update runs, as one with a tiny interval does, drops the rest of its debt.
            if (++timer.callsThisUpdate >= kMaxCallsPerUpdate && timer.remaining <= 0.0F) {
                timer.remaining = timer.interval;
            }
            timer.firedThisUpdate = timer.interval <= 0.0F;
            timer.cancelled = timer.repeatsLeft == 0;
            timer.callback();
            if (!timer.cancelled && !timer.paused && !timer.firedThisUpdate && timer.remaining <= 0.0F) {
                due.emplace_back(timer.remaining, index);
                std::ranges::push_heap(due, std::greater<>());
            }
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
