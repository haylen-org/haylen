#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "haylen/math/Easing.hpp"

namespace haylen::animation2d {

class Skeleton;

// One animation of a skeleton: timelines of keys that turn, move and scale bones, color slots and swap their attachments, deform meshes, change the mix of inverse kinematics, reorder the slots, and named events. Rotations, moves and scales are relative to the setup pose.
class SkeletonAnimation final {
  public:
    // How a key eases toward the next one: in a straight line, holding its value until the next key, through a named easing, or along a cubic Bezier curve.
    struct Curve {
        enum class Kind : std::uint8_t {
            Linear,
            Stepped,
            Eased,
            Bezier,
        };

        Kind kind = Kind::Linear;
        math::Easing::Type easing = math::Easing::Type::Linear;
        std::array<float, 4> bezier{};

        [[nodiscard]] float apply(float t) const noexcept;
    };

    struct Key {
        float time = 0.0F;
        std::array<float, 4> values{};
        Curve curve;
    };

    enum class Property : std::uint8_t {
        Rotate,
        Translate,
        Scale,
        Color,
        Attachment,
        Deform,
        IkMix,
    };

    // The keys of one property of one bone, slot or constraint. Attachment keys keep their names, and deform keys their vertex offsets for one attachment.
    struct Timeline {
        Property property = Property::Rotate;
        std::size_t target = 0;
        std::vector<Key> keys;
        std::vector<std::string> names;
        std::vector<std::vector<float>> offsets;
        std::string attachment;
    };

    struct DrawOrderKey {
        float time = 0.0F;
        std::vector<std::size_t> order;
    };

    struct Event {
        float time = 0.0F;
        std::string name;
        std::string text;
        float number = 0.0F;
    };

    std::string name;
    float duration = 0.0F;
    std::vector<Timeline> timelines;
    std::vector<DrawOrderKey> drawOrder;
    std::vector<Event> events;

    // Poses the skeleton at a time, mixed over its current pose by the share `alpha`. Attachments and the draw order change only from a share of one half on.
    void apply(Skeleton& skeleton, float time, bool loop, float alpha) const;

    // Appends the events whose time lies after `from` and up to `to`, including those of the passes a looping animation wraps through.
    void collectEvents(float from, float to, bool loop, std::vector<const Event*>& found) const;

  private:
    // Returns the key before the time and the eased share of the way to the next one, which is 0 before the first key and after the last.
    [[nodiscard]] static std::pair<std::size_t, float> locate(const std::vector<Key>& keys, float time) noexcept;
    [[nodiscard]] static float mixAngle(float from, float to, float alpha) noexcept;

    [[nodiscard]] float localTime(float time, bool loop) const noexcept;
};

} // namespace haylen::animation2d
