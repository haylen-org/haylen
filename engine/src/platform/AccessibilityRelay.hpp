#pragma once

#include <mutex>

#include "haylen/platform/Accessibility.hpp"

namespace haylen::platform {

// Carries the actions that screen readers ask of elements to the accessibility of the running engine, which the platform plugin attaches when the app starts. The platform asks from any thread, so the lock keeps the accessibility alive until a request has been queued, and a request that arrives while no app runs is dropped.
class AccessibilityRelay final {
  public:
    static void attach(Accessibility& value) noexcept;

    // Disconnects the relay when it still leads to this accessibility, so an engine that stops late never disconnects the one that replaced it.
    static void detach(const Accessibility& value) noexcept;

    static void perform(const Accessibility::Request& request);

  private:
    static std::mutex& mutex;
    static Accessibility* accessibility;
};

} // namespace haylen::platform
