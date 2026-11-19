#pragma once

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "haylen/core/FrameClock.hpp"
#include "haylen/core/PropertyTrack.hpp"
#include "haylen/core/PropertyTween.hpp"
#include "haylen/core/TweenManager.hpp"
#include "haylen/core/TweenProperty.hpp"
#include "haylen/core/TweenValue.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::core {

// Plays tweens on a clock that accepts long frames, so a test can jump through time in one step.
class TweenRig final {
  public:
    // A target of the tweens, with a number and a vector to animate.
    struct Box {
        double x = 0.0;
        math::Vec2 position{};
    };

    void step(double seconds) {
        clock.advance(seconds);
        tweens.update(clock);
    }

    // Returns a tween of the `x` of the box, added to the manager unless told otherwise.
    std::shared_ptr<PropertyTween> tweenX(Box& box, float duration, TweenProperty property, bool add = true) {
        auto tween = std::make_shared<PropertyTween>(duration);
        tween->addTrack(numberTrack(box, std::move(property)));

        if (add) {
            tweens.add(tween);
        }

        return tween;
    }

    static std::unique_ptr<PropertyTrack> numberTrack(Box& box, TweenProperty property, std::function<bool()> alive = {}) {
        return std::make_unique<PropertyTrack>(&box, std::vector<std::string>{"x"}, [&box] { return TweenValue(box.x); }, [&box](const TweenValue& value) { box.x = value.getNumber(); }, std::move(property), std::move(alive));
    }

    FrameClock clock{1.0 / 60.0, 100.0};
    TweenManager tweens;
};

} // namespace haylen::core
