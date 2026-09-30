#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "haylen/input/Gesture.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::input {

class Input;

// Turns touches, and the left mouse button when no finger is down, into taps, double taps, long presses, swipes and pinches, once per frame.
class GestureRecognizer final {
  public:
    // Distances are in design units and times in seconds.
    struct Settings {
        float tapMaxDuration = 0.3F;
        float tapMaxMovement = 24.0F;
        float doubleTapInterval = 0.35F;
        float doubleTapDistance = 48.0F;
        float longPressDuration = 0.5F;
        float swipeMinDistance = 90.0F;
        float swipeMaxDuration = 0.5F;
        bool mouse = true;
    };

    explicit GestureRecognizer(const Settings& value = kDefaultSettings) : settings(value) {}

    void update(const Input& input, float deltaSeconds);

    // Drops every finger, the mouse press and the pinch in progress without recognizing them, which the engine does together with `Input::releaseAll` when the app loses focus or leaves the foreground.
    void cancel() noexcept;

    [[nodiscard]] std::span<const Gesture> getGestures() const noexcept {
        return gestures;
    }

    [[nodiscard]] const Settings& getSettings() const noexcept {
        return settings;
    }
    void setSettings(const Settings& value) noexcept {
        settings = value;
    }

  private:
    struct Pointer {
        math::Vec2 start;
        math::Vec2 position;
        float duration = 0.0F;
        bool longPressed = false;
        bool shared = false;
    };

    static const Settings kDefaultSettings;

    void finish(const Pointer& pointer);
    void recognizePinch(const Input& input);

    Settings settings;
    std::vector<Gesture> gestures;
    std::map<std::uint64_t, Pointer> pointers;
    std::optional<Pointer> mouse;
    std::optional<std::pair<double, math::Vec2>> lastTap;
    std::optional<float> pinchStart;
    double time = 0.0;
};

} // namespace haylen::input
