#include "haylen/platform/AccessibilitySettings.hpp"

#include "haylen/core/JsonNumber.hpp"

namespace haylen::platform {

core::Json AccessibilitySettings::toJson() const {
    return {{"textScale", core::JsonNumber::fromFloat(textScale)}, {"reducedMotion", reducedMotion}, {"highContrast", highContrast}, {"reducedTransparency", reducedTransparency}, {"screenReader", screenReader}};
}

} // namespace haylen::platform
