#include "haylen/core/Tween.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

#include "haylen/core/Timeline.hpp"

namespace haylen::core {

debug::ObjectCounter& Tween::counter = *new debug::ObjectCounter("Tween", debug::ObjectCounter::Kind::Native);

void Tween::setDelay(float seconds) {
    if (!(seconds >= 0.0F)) {
        throw std::invalid_argument("A tween delay cannot be negative.");
    }
    delay = seconds;
    delayLeft = seconds;
}

void Tween::setRepeatCount(int count) noexcept {
    repeatCount = count;
}

void Tween::setRepeatDelay(float seconds) {
    if (!(seconds >= 0.0F)) {
        throw std::invalid_argument("A tween repeat delay cannot be negative.");
    }
    repeatDelay = seconds;
}

void Tween::setTimeScale(float value) {
    if (!(value >= 0.0F && std::isfinite(value))) {
        throw std::invalid_argument("A tween time scale must be a finite number of 0 or more.");
    }
    timeScale = value;
}

void Tween::setTag(std::string value) {
    if (manager != nullptr) {
        throw std::logic_error("The tag of a tween is fixed once it plays.");
    }
    tag = std::move(value);
}

Tween::Callbacks& Tween::getCallbacks() {
    if (!callbacks) {
        callbacks = std::make_shared<Callbacks>();
    }
    return *callbacks;
}

ProcessMode Tween::resolveProcessMode() const {
    return processMode == ProcessMode::Inherit && parentMode ? parentMode() : processMode;
}

float Tween::getTotalDuration() const noexcept {
    if (repeatCount < 0) {
        return std::numeric_limits<float>::infinity();
    }
    const auto loops = static_cast<float>(repeatCount + 1);
    return getDuration() * loops + repeatDelay * (loops - 1.0F);
}

Tween::Position Tween::locate(double totalTime) const noexcept {
    const float loopLength = getDuration();
    const double period = static_cast<double>(loopLength) + static_cast<double>(repeatDelay);
    if (period <= 0.0) {
        return {};
    }

    // The last loop of a tween that repeats forever stays below the largest int, so a step to the loop after it never overflows.
    const int lastLoop = repeatCount < 0 ? std::numeric_limits<int>::max() - 1 : repeatCount;
    const auto loop = static_cast<int>(std::min(std::floor(totalTime / period), static_cast<double>(lastLoop)));
    const auto inside = static_cast<float>(std::min(totalTime - static_cast<double>(loop) * period, static_cast<double>(loopLength)));
    return {.loop = loop, .time = loopMode == LoopMode::Yoyo && loop % 2 == 1 ? loopLength - inside : inside};
}

float Tween::getLoopStart(int loop) const noexcept {
    return loopMode == LoopMode::Yoyo && loop % 2 == 1 ? getDuration() : 0.0F;
}

float Tween::getLoopEnd(int loop) const noexcept {
    return loopMode == LoopMode::Yoyo && loop % 2 == 1 ? 0.0F : getDuration();
}

float Tween::getProgress() const noexcept {
    const float total = getTotalDuration();
    if (std::isinf(total)) {
        const float loopLength = getDuration();
        return loopLength > 0.0F ? locate(time).time / loopLength : 1.0F;
    }
    return total > 0.0F ? static_cast<float>(time / static_cast<double>(total)) : 1.0F;
}

float Tween::getRenderedProgress() const noexcept {
    const float loopLength = getDuration();
    return loopLength > 0.0F ? locate(time).time / loopLength : 1.0F;
}

void Tween::prepare() {}

void Tween::killChildren() {}

bool Tween::killTarget(const void*) {
    return false;
}

void Tween::yieldTo(const PropertyTween&) {}

void Tween::releasePlayback() noexcept {}

void Tween::discard() noexcept {
    killed = true;
    manager = nullptr;
    callbacks.reset();
    finished.clear();
}

void Tween::render(double totalTime, bool silent) {
    if (killed) {
        return;
    }
    const std::shared_ptr<const Callbacks> events = callbacks;
    const bool first = !rendered;
    if (first) {
        rendered = true;
        prepare();
        if (killed) {
            return;
        }
    }

    const auto total = static_cast<double>(getTotalDuration());
    const double target = std::clamp(totalTime, 0.0, total);
    const double previous = time;
    const bool forward = target >= previous;
    const Position left = locate(previous);
    const Position entered = locate(target);
    time = target;

    if (!started && forward && !silent) {
        started = true;
        if (events && events->start) {
            events->start();
        }
        if (killed) {
            return;
        }
    }

    // A loop that is left renders to its edge first, so a timeline finishes its tweens and callbacks before the next loop begins. The previous time of a loop that was just entered lies outside it, so callbacks on its edge run.
    const bool incremental = loopMode == LoopMode::Incremental;
    const float loopLength = getDuration();
    float previousInLoop = atStart && forward ? -1.0F : left.time;
    if (entered.loop != left.loop) {
        const float leftEdge = forward ? getLoopEnd(left.loop) : getLoopStart(left.loop);
        renderLoop(leftEdge, previousInLoop, incremental ? left.loop : 0, silent);
        if (killed) {
            return;
        }
        const int direction = forward ? 1 : -1;
        const int reported = std::abs(entered.loop - left.loop) > kMaxLoopCalls ? entered.loop - direction * (kMaxLoopCalls - 1) : left.loop + direction;
        for (int loop = reported; loop != entered.loop + direction && !silent && events && events->loop; loop += direction) {
            events->loop(loop);
            if (killed) {
                return;
            }
        }

        // Unless the loop turns around like a yoyo, the new loop begins at the other edge, so everything rewinds there silently first.
        const float edge = forward ? getLoopStart(entered.loop) : getLoopEnd(entered.loop);
        if (loopMode != LoopMode::Yoyo) {
            renderLoop(edge, leftEdge, incremental ? entered.loop : 0, true);
            if (killed) {
                return;
            }
        }
        previousInLoop = edge > 0.0F ? loopLength + 1.0F : -1.0F;
    }

    renderLoop(entered.time, previousInLoop, incremental ? entered.loop : 0, silent);
    if (killed) {
        return;
    }
    atStart = !forward && target <= 0.0;
    if (!silent && events && events->update) {
        events->update(getRenderedProgress());
        if (killed) {
            return;
        }
    }

    // A tween completes when it reaches its end in the direction it plays, and it stops counting as complete once it moves away.
    const bool atEnd = !std::isinf(total) && (reversed ? target <= 0.0 : target >= total);
    const bool arrived = atEnd && !completed && (first || (reversed ? previous > 0.0 : previous < total));
    completed = atEnd && (completed || arrived);
    if (!arrived) {
        return;
    }

    // Waiters hear of the completion before the `complete` callback runs, so a callback that kills the tween never turns its completion into a kill.
    finished.emit(true);
    if (killed) {
        return;
    }
    if (!silent && events && events->complete) {
        events->complete();
        if (killed) {
            return;
        }
    }
    if (autoKill && parent == nullptr) {
        end(true);
    }
}

void Tween::advance(float seconds) {
    float step = seconds * timeScale;
    if (!reversed && delayLeft > 0.0F) {
        delayLeft -= step;
        if (delayLeft > 0.0F) {
            return;
        }
        step = -delayLeft;
        delayLeft = 0.0F;
    }
    render(time + (reversed ? -step : step), false);
}

void Tween::play() {
    reversed = false;
    paused = false;
    completed = !std::isinf(getTotalDuration()) && time >= static_cast<double>(getTotalDuration()) && rendered;
}

void Tween::pause() noexcept {
    paused = true;
}

void Tween::resume() noexcept {
    paused = false;
}

void Tween::restart() {
    if (killed) {
        return;
    }
    delayLeft = delay;
    started = false;
    completed = false;
    reversed = false;
    paused = false;
    render(0.0F, true);
}

void Tween::reverse() {
    reversed = true;
    paused = false;
    delayLeft = 0.0F;
    completed = time <= 0.0;
}

void Tween::seek(float seconds) {
    if (std::isnan(seconds)) {
        throw std::invalid_argument("A tween cannot seek to a time that is not a number.");
    }
    if (killed) {
        return;
    }
    delayLeft = 0.0F;
    double target = std::max(seconds, 0.0F);
    if (std::isinf(getTotalDuration())) {
        const double period = static_cast<double>(getDuration()) + static_cast<double>(repeatDelay);
        target = static_cast<double>(locate(time).loop) * period + std::min(std::max(seconds, 0.0F), getDuration());
    }
    render(target, true);
}

void Tween::setProgress(float value) {
    const float clamped = std::clamp(value, 0.0F, 1.0F);
    const float total = getTotalDuration();
    seek(clamped * (std::isinf(total) ? getDuration() : total));
}

void Tween::complete(bool withCallbacks) {
    if (killed || std::isinf(getTotalDuration())) {
        return;
    }
    delayLeft = 0.0F;
    render(reversed ? 0.0F : getTotalDuration(), !withCallbacks);
}

void Tween::kill() {
    end(false);
}

void Tween::end(bool justCompleted) {
    if (killed) {
        return;
    }
    const std::shared_ptr<Tween> keepAlive = weak_from_this().lock();
    killed = true;
    killChildren();
    if (parent != nullptr) {
        parent->remove(*this);
    }

    // The finished signal always fires unless it just reported the completion, so a script waiting on it resumes even when the `kill` callback fails. A killed tween never calls anything again, so it lets go of its callbacks, its listeners and what it played with.
    const std::shared_ptr<const Callbacks> events = std::exchange(callbacks, nullptr);
    releasePlayback();
    std::exception_ptr failure;
    try {
        if (events && events->kill) {
            events->kill();
        }
    } catch (...) {
        failure = std::current_exception();
    }
    if (!justCompleted) {
        finished.emit(false);
    }
    finished.clear();
    if (failure) {
        std::rethrow_exception(failure);
    }
}

} // namespace haylen::core
