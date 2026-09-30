#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace haylen::core {

// Reads and writes the values that a tween animates on one target. The class `PropertyTrack` writes native C++ properties, and the Lua binding writes the fields of Lua tables and objects.
class TweenTrack {
  public:
    virtual ~TweenTrack() = default;

    // Resolves the start and end of every value from the current values of the target. It runs once, when the tween first renders.
    virtual void begin() = 0;

    virtual void render(float progress, int loops) = 0;

    // Returns `false` once the target is gone, which kills the tween before it writes again.
    [[nodiscard]] virtual bool isAlive() const = 0;

    // Returns the largest distance any value travels, which speed-based tweens divide by their speed.
    [[nodiscard]] virtual float getDistance() const = 0;

    // Identifies the target, so tweens can be killed by target and a newer tween can take over its fields.
    [[nodiscard]] virtual const void* getTarget() const noexcept = 0;
    [[nodiscard]] virtual std::vector<std::string> getFields() const = 0;

    // Stops writing the field, which a newer tween now owns, and returns whether the track still writes anything.
    virtual bool release(std::string_view field) = 0;
};

} // namespace haylen::core
