#include "haylen/core/Timeline.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

#include "haylen/core/PropertyTween.hpp"
#include "haylen/core/TweenManager.hpp"

namespace haylen::core {

Timeline::WalkGuard::WalkGuard(Timeline& owner) noexcept : timeline(owner) {
    ++timeline.walking;
}

Timeline::WalkGuard::~WalkGuard() {
    --timeline.walking;
    timeline.compact();
}

float Timeline::getExtent(const Tween& tween) noexcept {
    const float total = tween.getTotalDuration();
    return tween.getTimeScale() > 0.0F ? total / tween.getTimeScale() : std::numeric_limits<float>::infinity();
}

std::size_t Timeline::size() const noexcept {
    return static_cast<std::size_t>(std::count_if(children.begin(), children.end(), [](const Child& child) { return !child.removed; }));
}

void Timeline::place(std::shared_ptr<Tween> tween, float at, int step) {
    if (!tween) {
        throw std::invalid_argument("A timeline cannot hold an empty tween.");
    }
    if (!tween->isAlive() || tween->isInTimeline() || tween->hasRendered()) {
        throw std::logic_error("Only a tween that has not started and is in no other timeline can join a timeline.");
    }
    for (const Tween* ancestor = this; ancestor != nullptr; ancestor = ancestor->parent) {
        if (ancestor == tween.get()) {
            throw std::logic_error("A timeline cannot hold itself.");
        }
    }
    if (const auto* property = dynamic_cast<const PropertyTween*>(tween.get()); property != nullptr && property->isSpeedBased()) {
        throw std::logic_error("A speed-based tween cannot join a timeline, because its duration is only known when it starts.");
    }

    if (tween->manager != nullptr) {
        tween->manager->detach(*tween);
    }
    tween->parent = this;
    children.push_back({.tween = std::move(tween), .start = std::max(at, 0.0F), .step = step});
    children.back().start += children.back().tween->getDelay();
    updateDuration();
}

void Timeline::placeCall(std::function<void()> call, float at, int step) {
    if (!call) {
        throw std::invalid_argument("A timeline callback needs a function.");
    }
    children.push_back({.call = std::move(call), .start = std::max(at, 0.0F), .step = step});
    updateDuration();
}

Timeline& Timeline::append(std::shared_ptr<Tween> tween) {
    lastStepStart = duration;
    stepEnds.push_back(duration);
    place(std::move(tween), lastStepStart, static_cast<int>(stepEnds.size()) - 1);
    return *this;
}

Timeline& Timeline::join(std::shared_ptr<Tween> tween) {
    if (stepEnds.empty()) {
        return append(std::move(tween));
    }
    place(std::move(tween), lastStepStart, static_cast<int>(stepEnds.size()) - 1);
    return *this;
}

Timeline& Timeline::insert(float at, std::shared_ptr<Tween> tween) {
    place(std::move(tween), at, -1);
    return *this;
}

Timeline& Timeline::insert(std::string_view label, std::shared_ptr<Tween> tween) {
    return insert(getLabelTime(label), std::move(tween));
}

Timeline& Timeline::appendInterval(float seconds) {
    return append(std::make_shared<PropertyTween>(seconds));
}

Timeline& Timeline::appendCall(std::function<void()> call) {
    lastStepStart = duration;
    stepEnds.push_back(duration);
    placeCall(std::move(call), lastStepStart, static_cast<int>(stepEnds.size()) - 1);
    return *this;
}

Timeline& Timeline::joinCall(std::function<void()> call) {
    if (stepEnds.empty()) {
        return appendCall(std::move(call));
    }
    placeCall(std::move(call), lastStepStart, static_cast<int>(stepEnds.size()) - 1);
    return *this;
}

Timeline& Timeline::insertCall(float at, std::function<void()> call) {
    placeCall(std::move(call), at, -1);
    return *this;
}

Timeline& Timeline::addLabel(std::string name) {
    return addLabel(std::move(name), duration);
}

Timeline& Timeline::addLabel(std::string name, float at) {
    if (name.empty()) {
        throw std::invalid_argument("A timeline label needs a name.");
    }
    labels.insert_or_assign(std::move(name), std::max(at, 0.0F));
    return *this;
}

float Timeline::getLabelTime(std::string_view name) const {
    const auto found = labels.find(name);
    if (found == labels.end()) {
        throw std::invalid_argument("The timeline has no label named \"" + std::string(name) + "\".");
    }
    return found->second;
}

void Timeline::seekLabel(std::string_view name) {
    seek(getLabelTime(name));
}

Timeline& Timeline::stagger(std::vector<std::shared_ptr<Tween>> tweens, float each, StaggerOrigin origin) {
    if (tweens.empty()) {
        throw std::invalid_argument("A stagger needs at least one tween.");
    }
    lastStepStart = duration;
    stepEnds.push_back(duration);
    const int step = static_cast<int>(stepEnds.size()) - 1;
    const float base = lastStepStart;
    const float last = static_cast<float>(tweens.size() - 1);
    for (std::size_t index = 0; index < tweens.size(); ++index) {
        const auto position = static_cast<float>(index);
        float rank = position;
        if (origin == StaggerOrigin::End) {
            rank = last - position;
        } else if (origin == StaggerOrigin::Center) {
            rank = std::fabs(position - last * 0.5F);
        }
        place(std::move(tweens[index]), base + rank * each, step);
    }
    return *this;
}

void Timeline::updateDuration() {
    float end = 0.0F;
    std::fill(stepEnds.begin(), stepEnds.end(), 0.0F);
    for (const Child& child : children) {
        if (child.removed) {
            continue;
        }
        const float childEnd = child.start + (child.tween ? getExtent(*child.tween) : 0.0F);
        end = std::max(end, childEnd);
        if (child.step >= 0) {
            float& stepEnd = stepEnds[static_cast<std::size_t>(child.step)];
            stepEnd = std::max(stepEnd, childEnd);
        }
    }
    duration = end;
}

void Timeline::remove(const Tween& child) {
    for (Child& entry : children) {
        if (entry.tween.get() == &child) {
            entry.removed = true;
        }
    }
    compact();
}

void Timeline::compact() {
    if (walking > 0) {
        return;
    }
    const auto before = children.size();
    std::erase_if(children, [](const Child& child) { return child.removed; });
    if (children.size() != before) {
        updateDuration();
    }
}

void Timeline::renderLoop(float loopTime, float previous, int, bool silent) {
    const WalkGuard guard(*this);
    const bool forward = loopTime >= previous;
    const std::size_t count = children.size();

    // Items render in time order when the playhead moves forward and in reverse order when it moves back, so later tweens win over earlier ones in both directions.
    for (std::size_t visit = 0; visit < count && isAlive(); ++visit) {
        Child& child = children[forward ? visit : count - 1 - visit];
        if (child.removed) {
            continue;
        }
        if (child.call) {
            const bool crossed = forward ? previous < child.start && child.start <= loopTime : loopTime <= child.start && child.start < previous;
            if (crossed && !silent) {
                const std::function<void()> call = child.call;
                call();
            }
            continue;
        }

        // A tween that the playhead has not reached yet stays untouched, so it reads its start values only when its turn comes.
        Tween& tween = *child.tween;
        if (!tween.hasRendered() && loopTime < child.start) {
            continue;
        }
        const float local = std::clamp((loopTime - child.start) * tween.getTimeScale(), 0.0F, tween.getTotalDuration());
        if (!tween.hasRendered() || local != tween.time) {
            tween.render(local, silent);
        }
    }

    const Callbacks& events = getCallbacks();
    if (!forward || silent || !events.step) {
        return;
    }
    for (std::size_t step = 0; step < stepEnds.size() && isAlive(); ++step) {
        if (previous < stepEnds[step] && stepEnds[step] <= loopTime) {
            events.step(static_cast<int>(step));
        }
    }
}

void Timeline::killChildren() {
    const WalkGuard guard(*this);
    for (std::size_t index = 0; index < children.size(); ++index) {
        if (children[index].tween && !children[index].removed) {
            const std::shared_ptr<Tween> tween = children[index].tween;
            tween->kill();
        }
        children[index].removed = true;
    }
}

bool Timeline::killTarget(const void* target) {
    const WalkGuard guard(*this);
    for (std::size_t index = 0; index < children.size(); ++index) {
        if (children[index].tween && !children[index].removed) {
            const std::shared_ptr<Tween> tween = children[index].tween;
            (void)tween->killTarget(target);
        }
    }
    return false;
}

void Timeline::yieldTo(const PropertyTween& newer) {
    const WalkGuard guard(*this);
    for (std::size_t index = 0; index < children.size(); ++index) {
        if (children[index].tween && !children[index].removed) {
            const std::shared_ptr<Tween> tween = children[index].tween;
            tween->yieldTo(newer);
        }
    }
}

void Timeline::discard() noexcept {
    Tween::discard();
    for (Child& child : children) {
        if (child.tween) {
            child.tween->discard();
        }
    }
    children.clear();
    stepEnds.clear();
}

} // namespace haylen::core
