#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

#include "haylen/platform/AccessibilityElement.hpp"

namespace haylen::platform {

class WindowsAccessibilityTree;

// Serves the UI to the screen readers of Windows, such as Narrator, through UI Automation: the window of the app answers `WM_GETOBJECT` with the root provider of its tree, the elements act through `AccessibilityRelay`, and the tree raises the events of focus, structure and properties, and the notifications of announcements.
class WindowsAccessibility final {
  public:
    // Answers the request of UI Automation for the root of the window, and nothing for the other requests of `WM_GETOBJECT`, which the window passes on.
    [[nodiscard]] static std::optional<LRESULT> answer(HWND window, WPARAM wParam, LPARAM lParam);

    // Whether a screen reader runs or a client of UI Automation asked the window for its elements.
    [[nodiscard]] static bool isWanted() noexcept;

    static void update(std::span<const AccessibilityElement> elements, std::uint64_t focused);
    static void announce(std::string_view text, bool interrupt);

    // Lets go of the providers when the process ends, so clients that hold one hear that the window is gone.
    static void release() noexcept;

  private:
    [[nodiscard]] static WindowsAccessibilityTree& getTree(HWND window);

    static std::shared_ptr<WindowsAccessibilityTree> tree;
    static std::atomic<bool> asked;
};

} // namespace haylen::platform
