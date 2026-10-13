#pragma once

#include "haylen/core/Json.hpp"

namespace haylen::platform {

// The accessibility settings of the system as the platform last reported them, which the user may change while the app runs. A platform that has no such setting reports its default.
struct AccessibilitySettings {
    // How much larger the user wants text than its normal size, where 1 is the normal size.
    float textScale = 1.0F;
    bool reducedMotion = false;
    bool highContrast = false;
    bool reducedTransparency = false;

    // Whether a screen reader runs, which reads the interface aloud and moves through it by its own gestures.
    bool screenReader = false;

    [[nodiscard]] bool operator==(const AccessibilitySettings&) const = default;

    // The settings as the `accessibilitySettingsChanged` event and `accessibility.settings` report them.
    [[nodiscard]] core::Json toJson() const;
};

} // namespace haylen::platform
