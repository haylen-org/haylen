#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <ole2.h>

#include <uiautomation.h>
#include <wrl/client.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "haylen/math/Rect.hpp"
#include "haylen/platform/Accessibility.hpp"
#include "haylen/platform/AccessibilityElement.hpp"

namespace haylen::platform {

class WindowsAccessibilityElement;
class WindowsAccessibilityRoot;

// The tree of accessible elements of a window as UI Automation reads it, through a root provider for the window and one provider for each element that keeps its id across frames. UI Automation calls the providers from any thread, so the tree guards what it keeps with a lock, and it raises the events of UI Automation outside the lock, since a client may call a provider back while an event goes out.
class WindowsAccessibilityTree final : public std::enable_shared_from_this<WindowsAccessibilityTree> {
  public:
    using Performer = std::function<void(const Accessibility::Request&)>;

    WindowsAccessibilityTree(HWND owner, Performer performer);
    ~WindowsAccessibilityTree();

    WindowsAccessibilityTree(const WindowsAccessibilityTree&) = delete;
    WindowsAccessibilityTree& operator=(const WindowsAccessibilityTree&) = delete;

    // Takes the tree of a frame, with the size of the framebuffer its bounds are in, and raises the events of what changed while clients listen.
    void update(std::span<const AccessibilityElement> elements, std::uint64_t focused, math::Vec2 framebuffer);

    // Has screen readers say a text, after what they say or at once.
    void announce(std::string_view text, bool interrupt);

    void perform(std::uint64_t element, AccessibilityElement::Action action) const;

    [[nodiscard]] Microsoft::WRL::ComPtr<WindowsAccessibilityRoot> getRoot();
    [[nodiscard]] Microsoft::WRL::ComPtr<WindowsAccessibilityElement> getProvider(std::uint64_t id);
    [[nodiscard]] std::optional<AccessibilityElement> find(std::uint64_t id) const;
    [[nodiscard]] std::vector<std::uint64_t> getChildren(std::uint64_t parent) const;
    [[nodiscard]] std::uint64_t getFocused() const;

    // Returns the deepest element under a point of the screen, or 0 for none.
    [[nodiscard]] std::uint64_t findAt(math::Vec2 point) const;

    // Turns bounds in pixels of the framebuffer into pixels of the screen.
    [[nodiscard]] math::Rect toScreen(const math::Rect& bounds) const;

    [[nodiscard]] HWND getWindow() const noexcept {
        return window;
    }

    // Lets go of every provider, so clients that still hold one hear that its element is gone.
    void disconnect();

    // Converts UTF-8 to the UTF-16 of Windows, with a replacement character for every byte that is no UTF-8.
    [[nodiscard]] static std::wstring toWide(std::string_view text);

  private:
    // A change of an element whose event goes out once the lock is released: a property that changed, or an event of its own when `event` names one.
    struct Change {
        std::uint64_t id = 0;
        PROPERTYID property = 0;
        EVENTID event = 0;
        std::wstring before{};
        std::wstring after{};
        std::optional<BOOL> flagBefore{};
        std::optional<BOOL> flagAfter{};
        std::optional<int> stateBefore{};
        std::optional<int> stateAfter{};
        std::optional<double> numberBefore{};
        std::optional<double> numberAfter{};
    };

    [[nodiscard]] math::Rect toScreenLocked(const math::Rect& bounds) const;
    [[nodiscard]] std::uint64_t findAtLocked(std::uint64_t parent, math::Vec2 point) const;
    [[nodiscard]] static std::vector<Change> compare(const AccessibilityElement& before, const AccessibilityElement& after);
    void raise(const Change& change);

    HWND window;
    Performer performer;
    mutable std::mutex mutex;
    std::vector<AccessibilityElement> elements;
    std::unordered_map<std::uint64_t, std::size_t> indices;
    std::unordered_map<std::uint64_t, std::vector<std::uint64_t>> children;
    std::uint64_t focused = 0;
    float scale = 1.0F;
    Microsoft::WRL::ComPtr<WindowsAccessibilityRoot> root;
    std::unordered_map<std::uint64_t, Microsoft::WRL::ComPtr<WindowsAccessibilityElement>> providers;
};

} // namespace haylen::platform
