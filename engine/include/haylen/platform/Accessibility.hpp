#pragma once

#include <cstdint>
#include <mutex>
#include <span>
#include <string_view>
#include <vector>

#include "haylen/platform/AccessibilityElement.hpp"
#include "haylen/platform/AccessibilitySettings.hpp"

namespace haylen::core {
class EventBus;
}

namespace haylen::platform {

class Host;

// The accessibility of the running app: the settings of the system, which the engine publishes as `accessibilitySettingsChanged` once per change, the tree of elements that screen readers read, which the UI builds while the platform or the app wants it, the announcements of the app and the actions that screen readers ask for. Every method but `perform` runs on the frame thread.
class Accessibility final {
  public:
    // An action that a screen reader asked of an element, such as pressing a button.
    struct Request {
        std::uint64_t element = 0;
        AccessibilityElement::Action action = AccessibilityElement::Activate;
    };

    Accessibility(Host& host, core::EventBus& events);

    Accessibility(const Accessibility&) = delete;
    Accessibility& operator=(const Accessibility&) = delete;

    [[nodiscard]] const AccessibilitySettings& getSettings() const noexcept {
        return settings;
    }

    // Whether the UI builds the tree this frame: while the platform wants it, such as while a screen reader runs, or while the app turned it on, such as for tests.
    [[nodiscard]] bool isTreeWanted() const noexcept {
        return treeWanted || treeEnabled;
    }
    void setTreeEnabled(bool value) noexcept {
        treeEnabled = value;
    }
    [[nodiscard]] bool isTreeEnabled() const noexcept {
        return treeEnabled;
    }

    // Takes the tree the UI built this frame, with the id of the element that holds the focus or 0, and hands it to the platform when it changed since the last one it received.
    void publish(std::span<const AccessibilityElement> tree, std::uint64_t focused);
    [[nodiscard]] std::span<const AccessibilityElement> getElements() const noexcept {
        return elements;
    }
    [[nodiscard]] std::uint64_t getFocused() const noexcept {
        return focused;
    }

    // Asks the screen reader to say a text, after what it says now or interrupting it. Throws `std::invalid_argument` for an empty text.
    void announce(std::string_view text, bool interrupt);

    // Asks an element for an action from any thread, which the UI performs at its next frame.
    void perform(const Request& request);

    // Moves the requests that arrived since the last call into `taken`, which the caller reuses from frame to frame.
    void takeRequests(std::vector<Request>& taken);

    // Publishes the change of the settings that the platform reported since the last pump and reads whether the platform wants the tree. The engine calls it at the start of every frame.
    void pump();

  private:
    Host& host;
    core::EventBus& events;
    AccessibilitySettings settings;
    std::vector<AccessibilityElement> elements;
    std::uint64_t focused = 0;
    bool treeWanted = false;
    bool treeEnabled = false;
    std::mutex mutex;
    std::vector<Request> requests;
};

} // namespace haylen::platform
