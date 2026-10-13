#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "haylen/math/Rect.hpp"
#include "haylen/platform/AccessibilityElement.hpp"

namespace haylen::ui {

class Component;
class Context;
class FocusNavigator;

// Builds the elements that screen readers read from the GUIs as they draw. Every component that describes itself becomes an element inside the nearest element around it, the first focus target it registers is the one its actions reach, and parts of a component that are elements of their own, such as the options of a radio group, join with their own targets. The tree keeps its elements from frame to frame, so a frame that draws what the frame before drew allocates nothing.
class AccessibilityTree final {
  public:
    // Builds the elements of the frames that follow, which the UI does while the platform or the app wants them.
    void setEnabled(bool value) noexcept {
        enabled = value;
    }
    [[nodiscard]] bool isEnabled() const noexcept {
        return enabled;
    }

    void beginFrame() noexcept;

    // Wrap the drawing of a component, whose bounds are in UI coordinates.
    void enter(Context& context, const Component& component, ImGuiID node, const math::Rect& bounds);
    void leave(const FocusNavigator& focus);

    // Adds a part of the component being drawn as an element of its own, with the focus target that its actions reach, while the tree is enabled.
    void addPart(ImGuiID target, const math::Rect& bounds, platform::AccessibilityElement::Role role, std::string_view label, std::uint8_t states);

    // Marks the element whose target holds the focus.
    void endFrame(ImGuiID focusedItem);

    // The elements of the last frame, with bounds in UI coordinates until the UI turns them into framebuffer pixels.
    [[nodiscard]] std::span<platform::AccessibilityElement> getElements() noexcept {
        return {elements.data(), count};
    }
    [[nodiscard]] std::uint64_t getFocused() const noexcept {
        return focused;
    }

    // Append a number with the decimals, or the parts of a label separated by commas without the empty ones, to a text that keeps its storage, for what elements read aloud.
    static void appendNumber(std::string& text, double value, int decimals);
    static void appendParts(std::string& text, std::initializer_list<std::string_view> parts);

    // Returns the focus target that the actions of an element of the last frame reach.
    [[nodiscard]] std::optional<ImGuiID> findTarget(std::uint64_t element) const noexcept;

  private:
    struct Level {
        std::optional<std::size_t> element;
        std::size_t firstTarget = 0;
        bool hidden = false;
    };

    // Takes the next element of the frame, reusing the one an earlier frame kept in that place.
    platform::AccessibilityElement& add(std::uint64_t id, const math::Rect& bounds);
    [[nodiscard]] std::uint64_t getParent() const noexcept;

    std::vector<platform::AccessibilityElement> elements;
    std::vector<ImGuiID> targets;
    std::vector<Level> levels;
    std::size_t count = 0;
    std::uint64_t focused = 0;
    bool enabled = false;
};

} // namespace haylen::ui
