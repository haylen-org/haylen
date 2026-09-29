#include "haylen/core/TweenProperty.hpp"

#include <utility>

namespace haylen::core {

TweenProperty TweenProperty::to(TweenValue end) {
    return {Mode::To, {}, std::move(end)};
}

TweenProperty TweenProperty::from(TweenValue start) {
    return {Mode::From, std::move(start), {}};
}

TweenProperty TweenProperty::by(TweenValue offset) {
    return {Mode::By, {}, std::move(offset)};
}

TweenProperty TweenProperty::fromTo(TweenValue start, TweenValue end) {
    return {Mode::FromTo, std::move(start), std::move(end)};
}

void TweenProperty::begin(const TweenValue& current) {
    if (begun) {
        return;
    }
    switch (mode) {
    case Mode::To:
        start = current;
        break;
    case Mode::From:
        end = current;
        break;
    case Mode::By:
        end = TweenValue::add(current, end);
        start = current;
        break;
    case Mode::FromTo:
        break;
    }

    // Mixing checks the kinds, and doing it here reports a mismatch when the tween starts instead of in the middle of it.
    (void)TweenValue::mix(start, end, 0.0F, interpolation);
    if (motion) {
        motion->prepare(start, end);
    }
    begun = true;
}

TweenValue TweenProperty::evaluate(float progress, int loops) const {
    if (loops > 0 && start.getKind() != TweenValue::Kind::Text) {
        const TweenValue offset = TweenValue::difference(end, start);
        const auto times = static_cast<float>(loops);
        const TweenValue shiftedStart = TweenValue::add(start, offset, times);
        const TweenValue shiftedEnd = TweenValue::add(end, offset, times);
        return motion ? motion->evaluate(shiftedStart, shiftedEnd, progress) : TweenValue::mix(shiftedStart, shiftedEnd, progress, interpolation);
    }
    return motion ? motion->evaluate(start, end, progress) : TweenValue::mix(start, end, progress, interpolation);
}

float TweenProperty::getDistance() const {
    return motion ? motion->getDistance(start, end) : TweenValue::distance(start, end, interpolation);
}

} // namespace haylen::core
