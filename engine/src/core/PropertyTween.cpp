#include "haylen/core/PropertyTween.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

namespace haylen::core {

PropertyTween::PropertyTween(float length, bool perSecond) : duration(perSecond ? 0.0F : length), speed(perSecond ? length : 0.0F), speedBased(perSecond) {
    if (!(length > 0.0F)) {
        throw std::invalid_argument(perSecond ? "A speed-based tween needs a positive speed." : "A tween needs a positive duration.");
    }
}

void PropertyTween::addTrack(std::unique_ptr<TweenTrack> track) {
    if (!track) {
        throw std::invalid_argument("A tween track cannot be empty.");
    }
    if (hasRendered()) {
        throw std::logic_error("Tracks can only be added before the tween starts.");
    }
    tracks.push_back(std::move(track));
}

bool PropertyTween::areTargetsAlive() const {
    return std::all_of(tracks.begin(), tracks.end(), [](const std::unique_ptr<TweenTrack>& track) { return track->isAlive(); });
}

void PropertyTween::prepare() {
    if (begun) {
        return;
    }
    if (!areTargetsAlive()) {
        kill();
        return;
    }
    begun = true;
    for (const std::unique_ptr<TweenTrack>& track : tracks) {
        track->begin();
    }

    // A speed-based tween travels its longest value at the speed, and one that goes nowhere ends at once.
    if (speedBased) {
        float distance = 0.0F;
        for (const std::unique_ptr<TweenTrack>& track : tracks) {
            distance = std::max(distance, track->getDistance());
        }
        duration = std::max(distance / speed, 1e-4F);
    }
}

void PropertyTween::renderStart() {
    prepare();
    if (isAlive()) {
        renderLoop(0.0F, -1.0F, 0, true);
    }
}

void PropertyTween::renderLoop(float loopTime, float, int loops, bool) {
    if (!areTargetsAlive()) {
        kill();
        return;
    }
    easedProgress = ease.apply(duration > 0.0F ? loopTime / duration : 1.0F);
    for (const std::unique_ptr<TweenTrack>& track : tracks) {
        track->render(easedProgress, loops);
    }
}

bool PropertyTween::killTarget(const void* target) {
    const bool animates = std::any_of(tracks.begin(), tracks.end(), [target](const std::unique_ptr<TweenTrack>& track) { return track->getTarget() == target; });
    if (animates) {
        kill();
    }
    return animates;
}

void PropertyTween::yieldTo(const PropertyTween& newer) {
    if (&newer == this) {
        return;
    }
    for (const std::unique_ptr<TweenTrack>& incoming : newer.tracks) {
        for (const std::string& field : incoming->getFields()) {
            for (const std::unique_ptr<TweenTrack>& track : tracks) {
                if (track->getTarget() == incoming->getTarget()) {
                    (void)track->release(field);
                }
            }
        }
    }

    // A tween that animated values and has none left has nothing to do anymore.
    const bool empty = std::all_of(tracks.begin(), tracks.end(), [](const std::unique_ptr<TweenTrack>& track) { return track->getFields().empty(); });
    if (!tracks.empty() && empty) {
        kill();
    }
}

void PropertyTween::discard() noexcept {
    Tween::discard();
    tracks.clear();
}

} // namespace haylen::core
