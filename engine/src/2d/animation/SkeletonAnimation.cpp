#include "haylen/2d/animation/SkeletonAnimation.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "haylen/2d/animation/Skeleton.hpp"

namespace haylen::animation2d {

float SkeletonAnimation::Curve::apply(float t) const noexcept {
    switch (kind) {
    case Kind::Linear:
        return t;
    case Kind::Stepped:
        return 0.0F;
    case Kind::Eased:
        return math::Easing::apply(easing, t);
    case Kind::Bezier:
        break;
    }
    return math::Easing::cubicBezier(t, bezier[0], bezier[1], bezier[2], bezier[3]);
}

std::pair<std::size_t, float> SkeletonAnimation::locate(const std::vector<Key>& keys, float time) noexcept {
    const auto next = std::ranges::upper_bound(keys, time, {}, &Key::time);
    if (next == keys.begin()) {
        return {0, 0.0F};
    }
    const auto index = static_cast<std::size_t>(next - keys.begin()) - 1;
    if (next == keys.end()) {
        return {index, 0.0F};
    }
    const float span = next->time - keys[index].time;
    return {index, keys[index].curve.apply(span > 0.0F ? (time - keys[index].time) / span : 0.0F)};
}

// Angles mix the short way around the circle.
float SkeletonAnimation::mixAngle(float from, float to, float alpha) noexcept {
    constexpr float kTurn = std::numbers::pi_v<float> * 2.0F;
    float delta = std::fmod(to - from, kTurn);
    if (delta > std::numbers::pi_v<float>) {
        delta -= kTurn;
    } else if (delta < -std::numbers::pi_v<float>) {
        delta += kTurn;
    }
    return from + delta * alpha;
}

float SkeletonAnimation::localTime(float time, bool loop) const noexcept {
    if (duration <= 0.0F) {
        return 0.0F;
    }
    return loop ? std::fmod(std::max(0.0F, time), duration) : std::clamp(time, 0.0F, duration);
}

void SkeletonAnimation::apply(Skeleton& skeleton, float time, bool loop, float alpha) const {
    const float at = localTime(time, loop);
    const bool switches = alpha >= 0.5F;
    const SkeletonData& data = *skeleton.data;

    for (const Timeline& timeline : timelines) {
        if (timeline.keys.empty()) {
            continue;
        }
        const auto [index, share] = locate(timeline.keys, at);
        const Key& key = timeline.keys[index];
        const Key& next = timeline.keys[std::min(index + 1, timeline.keys.size() - 1)];
        const auto value = [&key, &next, share](std::size_t component) { return key.values[component] + (next.values[component] - key.values[component]) * share; };

        switch (timeline.property) {
        case Property::Rotate: {
            Skeleton::BonePose& bone = skeleton.bones[timeline.target];
            bone.rotation = mixAngle(bone.rotation, data.getBones()[timeline.target].rotation + value(0), alpha);
            break;
        }
        case Property::Translate: {
            Skeleton::BonePose& bone = skeleton.bones[timeline.target];
            const math::Vec2 keyed = data.getBones()[timeline.target].position + math::Vec2{value(0), value(1)};
            bone.position += (keyed - bone.position) * alpha;
            break;
        }
        case Property::Scale: {
            Skeleton::BonePose& bone = skeleton.bones[timeline.target];
            const math::Vec2 keyed = data.getBones()[timeline.target].scale * math::Vec2{value(0), value(1)};
            bone.scale += (keyed - bone.scale) * alpha;
            break;
        }
        case Property::Color: {
            math::Color& slot = skeleton.slots[timeline.target].color;
            slot = math::Color::lerp(slot, {value(0), value(1), value(2), value(3)}, alpha);
            break;
        }
        case Property::Attachment:
            if (switches) {
                const std::string& attachment = timeline.names[index];
                skeleton.slots[timeline.target].attachment = attachment.empty() ? nullptr : data.findAttachment(skeleton.skin, timeline.target, attachment);
            }
            break;
        case Property::Deform: {
            Skeleton::SlotPose& slot = skeleton.slots[timeline.target];
            if (slot.attachment == nullptr || slot.attachment->name != timeline.attachment) {
                break;
            }
            const std::vector<float>& from = timeline.offsets[index];
            const std::vector<float>& to = timeline.offsets[std::min(index + 1, timeline.offsets.size() - 1)];
            slot.deform.resize(from.size(), 0.0F);
            for (std::size_t place = 0; place < from.size(); ++place) {
                const float keyed = from[place] + (to[place] - from[place]) * share;
                slot.deform[place] += (keyed - slot.deform[place]) * alpha;
            }
            break;
        }
        case Property::IkMix:
            skeleton.ikMixes[timeline.target] += (value(0) - skeleton.ikMixes[timeline.target]) * alpha;
            if (switches) {
                skeleton.ikBends[timeline.target] = key.values[1] != 0.0F;
            }
            break;
        }
    }

    if (switches && !drawOrder.empty()) {
        const auto next = std::ranges::upper_bound(drawOrder, at, {}, &DrawOrderKey::time);
        if (next != drawOrder.begin()) {
            skeleton.drawOrder = (next - 1)->order;
        }
    }
}

void SkeletonAnimation::collectEvents(float from, float to, bool loop, std::vector<const Event*>& found) const {
    if (events.empty() || to <= from) {
        return;
    }
    if (!loop || duration <= 0.0F) {
        for (const Event& event : events) {
            if (event.time > from && event.time <= to) {
                found.push_back(&event);
            }
        }
        return;
    }

    // Each pass the times cross reports its events in order, with at most three passes for a step longer than the animation.
    const float firstPass = std::max(std::floor(from / duration), std::floor(to / duration) - 2.0F);
    for (float pass = firstPass; pass <= std::floor(to / duration); pass += 1.0F) {
        const float base = pass * duration;
        for (const Event& event : events) {
            const float at = base + event.time;
            if (at > from && at <= to) {
                found.push_back(&event);
            }
        }
    }
}

} // namespace haylen::animation2d
