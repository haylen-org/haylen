#pragma once

#include <cstdint>
#include <memory>
#include <utility>

#include "haylen/core/TweenMotion.hpp"
#include "haylen/core/TweenValue.hpp"

namespace haylen::core {

// One value that a tween animates: how its start and end are found, how it travels between them and, optionally, the motion it follows instead of a straight line. Tracks read and write the value, and the property computes it from the eased progress.
class TweenProperty final {
  public:
    // The mode `To` ends at the given value and `From` starts at it, `By` ends at the current value plus the given offset, and `FromTo` gives both ends. The current value is read when the tween first renders, except for `From`, whose end is the value the target has when the tween is created.
    enum class Mode : std::uint8_t {
        To,
        From,
        By,
        FromTo,
    };

    [[nodiscard]] static TweenProperty to(TweenValue end);
    [[nodiscard]] static TweenProperty from(TweenValue start);
    [[nodiscard]] static TweenProperty by(TweenValue offset);
    [[nodiscard]] static TweenProperty fromTo(TweenValue start, TweenValue end);

    void setInterpolation(TweenValue::Interpolation value) noexcept {
        interpolation = value;
    }
    [[nodiscard]] TweenValue::Interpolation getInterpolation() const noexcept {
        return interpolation;
    }
    void setMotion(std::shared_ptr<TweenMotion> value) noexcept {
        motion = std::move(value);
    }

    [[nodiscard]] Mode getMode() const noexcept {
        return mode;
    }

    // Resolves the start and end from the current value of the target, once.
    void begin(const TweenValue& current);

    // Computes the value at the eased progress. The parameter `loops` is the number of completed loops of an incremental tween, which shifts the range forward so every loop continues from where the previous one ended. Texts start over on every loop.
    [[nodiscard]] TweenValue evaluate(float progress, int loops) const;

    [[nodiscard]] float getDistance() const;

  private:
    TweenProperty(Mode kind, TweenValue first, TweenValue second) : mode(kind), start(std::move(first)), end(std::move(second)) {}

    Mode mode;
    TweenValue start;
    TweenValue end;
    TweenValue::Interpolation interpolation = TweenValue::Interpolation::Linear;
    std::shared_ptr<TweenMotion> motion;
    bool begun = false;
};

} // namespace haylen::core
