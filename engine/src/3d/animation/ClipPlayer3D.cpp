#include "haylen/3d/animation/ClipPlayer3D.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <stdexcept>

namespace haylen::animation3d {

const std::array<std::pair<std::string_view, ClipPlayer3D::Loop>, 3> ClipPlayer3D::kLoopNames{{{"loop", Loop::Loop}, {"once", Loop::Once}, {"pingPong", Loop::PingPong}}};

std::optional<ClipPlayer3D::Loop> ClipPlayer3D::loopFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kLoopNames, name, &std::pair<std::string_view, Loop>::first);
    return found != kLoopNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view ClipPlayer3D::loopName(Loop value) noexcept {
    return std::ranges::find(kLoopNames, value, &std::pair<std::string_view, Loop>::second)->first;
}

ClipPlayer3D::ClipPlayer3D(std::shared_ptr<const Clip3D> playedClip) : ClipPlayer3D(std::move(playedClip), Options{}) {}

ClipPlayer3D::ClipPlayer3D(std::shared_ptr<const Clip3D> playedClip, const Options& options) : loop(options.loop), paused(options.paused) {
    setSpeed(options.speed);
    setClip(std::move(playedClip));
    if (options.time) {
        setTime(*options.time);
    }
}

void ClipPlayer3D::setClip(std::shared_ptr<const Clip3D> value) {
    if (!value) {
        throw std::invalid_argument("A clip player needs a clip to play.");
    }

    clip = std::move(value);
    values.assign(clip->getValueCount(), 0.0F);
    cursors.assign(clip->getTracks().size(), 0);
    phase = speed < 0.0F ? clip->getDuration() : 0.0F;
    finished = false;
    fresh = true;
    resample();
}

float ClipPlayer3D::getPeriod() const noexcept {
    const float duration = clip->getDuration();
    return loop == Loop::PingPong ? duration * 2.0F : duration;
}

float ClipPlayer3D::timeOfPhase(float value) const noexcept {
    const float duration = clip->getDuration();
    return value <= duration ? value : duration * 2.0F - value;
}

float ClipPlayer3D::getTime() const noexcept {
    return timeOfPhase(phase);
}

void ClipPlayer3D::setTime(float value) {
    if (!std::isfinite(value) || value < 0.0F || value > clip->getDuration()) {
        throw std::invalid_argument(std::format("The time {} lies outside the clip \"{}\", which lasts {} seconds.", value, clip->getName(), clip->getDuration()));
    }

    phase = value;
    finished = false;
    fresh = true;
    resample();
}

void ClipPlayer3D::setSpeed(float value) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument(std::format("The speed of a clip player is a finite number, while it was given {}.", value));
    }

    speed = value;
}

void ClipPlayer3D::setLoop(Loop value) noexcept {
    phase = getTime();
    loop = value;
}

void ClipPlayer3D::update(float deltaSeconds) {
    if (emitting) {
        throw std::logic_error("A clip player cannot update from a listener of its own events.");
    }

    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0F) {
        throw std::invalid_argument(std::format("A clip player updates by a finite time step of zero or more seconds, while it was given {}.", deltaSeconds));
    }

    if (paused) {
        return;
    }

    passed.clear();
    const bool completes = advance(deltaSeconds * speed);
    resample();
    if (passed.empty() && !completes) {
        return;
    }

    // A listener may set another clip, so the events stay those of the clip that played them.
    const std::shared_ptr<const Clip3D> played = clip;
    emitting = true;
    try {
        for (const std::size_t event : passed) {
            eventFired.emit(played->getEvents()[event]);
        }

        if (completes) {
            completed.emit();
        }
    } catch (...) {
        emitting = false;
        throw;
    }

    emitting = false;
}

void ClipPlayer3D::resample() {
    clip->sample(getTime(), values, cursors);
}

bool ClipPlayer3D::advance(float amount) {
    // A clip without length reports its events once, as the player first updates.
    const float duration = clip->getDuration();
    if (duration == 0.0F) {
        if (!fresh) {
            return false;
        }

        collect(0.0F, 0.0F, true);
        fresh = false;
        const bool completes = loop == Loop::Once && !finished;
        finished = finished || completes;
        return completes;
    }

    if (amount == 0.0F) {
        return false;
    }

    if (loop == Loop::Once) {
        const float end = std::clamp(phase + amount, 0.0F, duration);
        if (end == phase) {
            return false;
        }

        collect(phase, end, fresh);
        phase = end;
        fresh = false;
        const bool reached = amount > 0.0F ? end == duration : end == 0.0F;
        const bool completes = reached && !finished;
        finished = reached;
        return completes;
    }

    if (amount > 0.0F) {
        advanceForward(amount);
    } else if (amount < 0.0F) {
        advanceBackward(amount);
    }

    return false;
}

// An update that wraps passes the rest of the cycle, whole cycles and the start of the last one. Only the last passes report their events, and the remainder of a float division keeps the phase exact at any distance.
void ClipPlayer3D::advanceForward(float amount) {
    const float period = getPeriod();
    const float end = phase + amount;
    if (end <= period) {
        collect(phase, end, fresh);
        phase = end;
        fresh = false;
        return;
    }

    float last = std::fmod(end, period);
    if (last == 0.0F) {
        last = period;
    }

    const double whole = std::round((static_cast<double>(end) - last) / period) - 1.0;
    const bool reportsAll = whole + 2.0 <= kMaxReportedPasses;
    const int reported = reportsAll ? static_cast<int>(whole) : kMaxReportedPasses - 1;
    if (reportsAll) {
        collect(phase, period, fresh);
    }

    for (int pass = 0; pass < reported; ++pass) {
        collect(0.0F, period, true);
    }

    collect(0.0F, last, true);
    phase = last;
    fresh = false;
}

void ClipPlayer3D::advanceBackward(float amount) {
    const float period = getPeriod();
    const float end = phase + amount;
    if (end >= 0.0F) {
        collect(phase, end, fresh);
        phase = end;
        fresh = false;
        return;
    }

    float last = std::fmod(-end, period);
    if (last == 0.0F) {
        last = period;
    }

    const double whole = std::round((static_cast<double>(-end) - last) / period);
    const bool reportsAll = whole + 2.0 <= kMaxReportedPasses;
    const int reported = reportsAll ? static_cast<int>(whole) : kMaxReportedPasses - 1;
    if (reportsAll) {
        collect(phase, 0.0F, fresh);
    }

    for (int pass = 0; pass < reported; ++pass) {
        collect(period, 0.0F, true);
    }

    collect(period, period - last, true);
    phase = period - last;
    fresh = false;
}

// The events of a player that turns at the ends lie at their times going forward and again, between the ends, at the mirrored phases coming back, so the phases of the cycle run through them in order.
void ClipPlayer3D::collect(float from, float to, bool withStart) {
    const std::span<const Clip3D::Event> events = clip->getEvents();
    const bool forward = to > from || (to == from && speed >= 0.0F);
    const float duration = clip->getDuration();
    const bool turns = loop == Loop::PingPong;
    const std::size_t count = events.size();
    if (forward) {
        for (std::size_t event = 0; event < count; ++event) {
            collectAt(events[event].time, event, from, to, true, withStart);
        }

        for (std::size_t event = count; turns && event > 0; --event) {
            const float time = events[event - 1].time;
            if (time > 0.0F && time < duration) {
                collectAt(duration * 2.0F - time, event - 1, from, to, true, withStart);
            }
        }

        return;
    }

    for (std::size_t event = 0; turns && event < count; ++event) {
        const float time = events[event].time;
        if (time > 0.0F && time < duration) {
            collectAt(duration * 2.0F - time, event, from, to, false, withStart);
        }
    }

    for (std::size_t event = count; event > 0; --event) {
        collectAt(events[event - 1].time, event - 1, from, to, false, withStart);
    }
}

void ClipPlayer3D::collectAt(float at, std::size_t event, float from, float to, bool forward, bool withStart) {
    const bool afterStart = forward ? at > from : at < from;
    const bool beforeEnd = forward ? at <= to : at >= to;
    if ((afterStart || (withStart && at == from)) && beforeEnd) {
        passed.push_back(event);
    }
}

} // namespace haylen::animation3d
