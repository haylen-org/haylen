#include "haylen/platform/Accessibility.hpp"

#include "haylen/core/EventBus.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "platform/Host.hpp"

namespace haylen::platform {

Accessibility::Accessibility(Host& owner, core::EventBus& bus) : host(owner), events(bus), settings(owner.getAccessibilitySettings()) {}

void Accessibility::pump() {
    const AccessibilitySettings reported = host.getAccessibilitySettings();
    if (reported != settings) {
        settings = reported;
        events.emit(core::LifecycleEvent::kAccessibilitySettingsChanged, settings.toJson());
    }
}

} // namespace haylen::platform
