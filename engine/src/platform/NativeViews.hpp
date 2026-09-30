#pragma once

#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <string_view>

#include "haylen/math/Insets.hpp"

namespace haylen::platform {

// What the native views of plugins over the app tell the engine: the screen edges they reserve, by the key of each view in framebuffer pixels, and how many of them cover the app, such as full screen ads and sign-in forms. Native code calls it from any thread, and the engine reads it on the frame thread once per frame.
class NativeViews final {
  public:
    // Reserves the edges that the view named by `key` takes, replacing its earlier reservation. Negative insets reserve nothing.
    void reserveInsets(std::string_view key, const math::Insets& insets);
    void releaseInsets(std::string_view key);

    // The largest reservation on each edge.
    [[nodiscard]] math::Insets getReservedInsets() const;

    // Counts the native UI that covers the app. Every `coverApp` takes its own `uncoverApp`, and an `uncoverApp` without one is logged and ignored.
    void coverApp();
    void uncoverApp();
    [[nodiscard]] bool isAppCovered() const;

  private:
    mutable std::mutex mutex;
    std::map<std::string, math::Insets, std::less<>> reservations;
    int covers = 0;
};

} // namespace haylen::platform
