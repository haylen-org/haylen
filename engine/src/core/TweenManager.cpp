#include "haylen/core/TweenManager.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <utility>

#include "haylen/core/PropertyTween.hpp"

namespace haylen::core {

TweenManager::~TweenManager() {
    clear();
}

std::size_t TweenManager::getGroup(std::string_view tag) {
    const auto found = std::find_if(groups.begin(), groups.end(), [tag](const Group& group) { return group.tag == tag; });
    if (found != groups.end()) {
        return static_cast<std::size_t>(found - groups.begin());
    }
    groups.push_back({.tag = std::string(tag)});
    return groups.size() - 1;
}

void TweenManager::add(std::shared_ptr<Tween> tween) {
    if (!tween) {
        throw std::invalid_argument("The tween manager cannot play an empty tween.");
    }
    if (!tween->isAlive() || tween->manager != nullptr || tween->isInTimeline()) {
        throw std::logic_error("A tween plays in one manager or timeline at a time, and a killed tween cannot play again.");
    }
    tween->manager = this;
    tween->group = getGroup(tween->getTag());

    // Tweens added from a callback wait for the next update so the running iteration stays valid.
    (updating ? pending : tweens).push_back(std::move(tween));
}

void TweenManager::detach(Tween& tween) {
    tween.manager = nullptr;
    if (updating) {
        return;
    }
    for (auto* list : {&tweens, &pending}) {
        std::erase_if(*list, [&tween](const std::shared_ptr<Tween>& candidate) { return candidate.get() == &tween; });
    }
}

void TweenManager::advanceAll(const FrameClock& clock, bool fixed) {
    updating = true;
    try {
        const std::size_t count = tweens.size();
        for (std::size_t index = 0; index < count; ++index) {
            Tween& tween = *tweens[index];
            if (tween.manager != this || !tween.isPlaying() || tween.isFixedStep() != fixed || !clock.canProcess(tween.getProcessMode())) {
                continue;
            }
            const double delta = fixed ? clock.getFixedStep() : (tween.isUnscaledTime() ? clock.getUnscaledDelta() : clock.getDelta());
            tween.advance(static_cast<float>(delta) * groups[tween.group].timeScale);
        }
    } catch (...) {
        finishUpdate();
        throw;
    }
    finishUpdate();
}

void TweenManager::update(const FrameClock& clock) {
    advanceAll(clock, false);
}

void TweenManager::fixedUpdate(const FrameClock& clock) {
    advanceAll(clock, true);
}

void TweenManager::finishUpdate() {
    updating = false;
    std::erase_if(tweens, [this](const std::shared_ptr<Tween>& tween) { return !tween->isAlive() || tween->manager != this; });
    std::move(pending.begin(), pending.end(), std::back_inserter(tweens));
    pending.clear();
}

void TweenManager::overwrite(const PropertyTween& tween) {
    for (const auto* list : {&tweens, &pending}) {
        const std::vector<std::shared_ptr<Tween>> snapshot = *list;
        for (const std::shared_ptr<Tween>& candidate : snapshot) {
            if (candidate->isAlive()) {
                candidate->yieldTo(tween);
            }
        }
    }
}

void TweenManager::killTarget(const void* target) {
    for (const auto* list : {&tweens, &pending}) {
        const std::vector<std::shared_ptr<Tween>> snapshot = *list;
        for (const std::shared_ptr<Tween>& candidate : snapshot) {
            if (candidate->isAlive()) {
                (void)candidate->killTarget(target);
            }
        }
    }
}

std::vector<std::shared_ptr<Tween>> TweenManager::collectTagged(std::string_view tag) const {
    std::vector<std::shared_ptr<Tween>> tagged;
    for (const auto* list : {&tweens, &pending}) {
        std::copy_if(list->begin(), list->end(), std::back_inserter(tagged), [tag, this](const std::shared_ptr<Tween>& tween) { return tween->isAlive() && tween->manager == this && tween->getTag() == tag; });
    }
    return tagged;
}

void TweenManager::killTag(std::string_view tag) {
    for (const std::shared_ptr<Tween>& tween : collectTagged(tag)) {
        tween->kill();
    }
}

void TweenManager::completeTag(std::string_view tag, bool withCallbacks) {
    for (const std::shared_ptr<Tween>& tween : collectTagged(tag)) {
        tween->complete(withCallbacks);
    }
}

void TweenManager::pauseTag(std::string_view tag, bool paused) {
    for (const std::shared_ptr<Tween>& tween : collectTagged(tag)) {
        if (paused) {
            tween->pause();
        } else {
            tween->resume();
        }
    }
}

void TweenManager::setTimeScale(std::string_view tag, float value) {
    if (value < 0.0F) {
        throw std::invalid_argument("A tween time scale cannot be negative.");
    }
    groups[getGroup(tag)].timeScale = value;
}

float TweenManager::getTimeScale(std::string_view tag) const noexcept {
    const auto found = std::find_if(groups.begin(), groups.end(), [tag](const Group& group) { return group.tag == tag; });
    return found != groups.end() ? found->timeScale : 1.0F;
}

void TweenManager::killAll() {
    std::vector<std::shared_ptr<Tween>> all;
    for (const auto* list : {&tweens, &pending}) {
        std::copy_if(list->begin(), list->end(), std::back_inserter(all), [this](const std::shared_ptr<Tween>& tween) { return tween->isAlive() && tween->manager == this; });
    }
    for (const std::shared_ptr<Tween>& tween : all) {
        tween->kill();
    }
}

void TweenManager::clear() noexcept {
    for (auto* list : {&tweens, &pending}) {
        for (const std::shared_ptr<Tween>& tween : *list) {
            if (tween->manager == this) {
                tween->discard();
            }
        }
    }
    pending.clear();
    if (!updating) {
        tweens.clear();
    }
}

std::size_t TweenManager::size() const noexcept {
    std::size_t count = 0;
    for (const auto* list : {&tweens, &pending}) {
        count += static_cast<std::size_t>(std::count_if(list->begin(), list->end(), [this](const std::shared_ptr<Tween>& tween) { return tween->isAlive() && tween->manager == this; }));
    }
    return count;
}

} // namespace haylen::core
