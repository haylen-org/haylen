#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "haylen/platform/AccessibilityElement.hpp"

namespace haylen::port {

// The screen reader of a platform, which reads the tree of accessible elements that the UI builds and says announcements. A port registers it where its platform has a screen reader that apps reach, and the engine never builds the tree for a platform without it.
class AccessibilityService {
  public:
    AccessibilityService() = default;
    AccessibilityService(const AccessibilityService&) = delete;
    AccessibilityService& operator=(const AccessibilityService&) = delete;
    virtual ~AccessibilityService() = default;

    // Whether the platform wants the tree, such as while a screen reader runs, which the engine reads once per frame.
    [[nodiscard]] virtual bool isTreeWanted() = 0;

    // Hands the platform the tree whenever it changed, with the id of the element that holds the focus or 0, and the platform answers the actions of the screen reader through `SystemReports::performAccessibility`.
    virtual void update(std::span<const platform::AccessibilityElement> elements, std::uint64_t focused) = 0;

    // Asks the screen reader to say a text, after what it says now or interrupting it.
    virtual void announce(std::string_view text, bool interrupt) = 0;
};

} // namespace haylen::port
