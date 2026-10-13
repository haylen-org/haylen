#include "haylen/ui/AccessibilityTree.hpp"

#include <array>
#include <charconv>

#include "haylen/ui/Component.hpp"
#include "haylen/ui/Context.hpp"
#include "haylen/ui/FocusNavigator.hpp"

namespace haylen::ui {

using Element = platform::AccessibilityElement;

void AccessibilityTree::beginFrame() noexcept {
    count = 0;
    focused = 0;
    levels.clear();
}

// A node that the app hides from screen readers hides its children too, and a node that its kind leaves out still becomes an element when the app gives it a role or a label.
void AccessibilityTree::enter(Context& context, const Component& component, ImGuiID node, const math::Rect& bounds) {
    const Component::CommonProperties& common = component.getCommon();
    if ((!levels.empty() && levels.back().hidden) || common.accessibilityHidden) {
        levels.push_back({.hidden = true});
        return;
    }

    const std::size_t index = count;
    Element& element = add(node, bounds);
    const bool described = component.describe(context, element);
    if (!described && common.accessibilityLabel.isEmpty() && !common.accessibilityRole) {
        --count;
        levels.push_back({.firstTarget = context.getFocus().getTargetCount()});
        return;
    }
    if (common.accessibilityRole) {
        element.role = *common.accessibilityRole;
    }
    if (!common.accessibilityLabel.isEmpty()) {
        element.label.assign(context.getText(common.accessibilityLabel));
    }
    if (!common.accessibilityHint.isEmpty()) {
        element.hint.assign(context.getText(common.accessibilityHint));
    } else if (!common.tooltip.isEmpty()) {
        element.hint.assign(context.getText(common.tooltip));
    }
    if (!common.enabled) {
        element.states |= Element::Disabled;
    }
    levels.push_back({.element = index, .firstTarget = context.getFocus().getTargetCount()});
}

// The first focus target the element registered is the one its actions reach, and an element without one, such as a disabled control, offers no action.
void AccessibilityTree::leave(const FocusNavigator& focus) {
    const Level level = levels.back();
    levels.pop_back();
    if (!level.element) {
        return;
    }
    Element& element = elements[*level.element];
    if (element.actions == 0) {
        return;
    }
    if (targets[*level.element] == 0 && focus.getTargetCount() > level.firstTarget) {
        targets[*level.element] = focus.getTargetId(level.firstTarget);
    }
    if (targets[*level.element] == 0) {
        element.actions = 0;
        return;
    }
    element.actions |= Element::Focus;
}

void AccessibilityTree::addPart(ImGuiID target, const math::Rect& bounds, Element::Role role, std::string_view label, std::uint8_t states) {
    if (!enabled || (!levels.empty() && levels.back().hidden)) {
        return;
    }
    Element& element = add(target, bounds);
    element.role = role;
    element.label.assign(label);
    element.states = states;
    if (target != 0 && (states & Element::Disabled) == 0) {
        element.actions = Element::Focus | Element::Activate;
        targets[count - 1] = target;
    }
}

void AccessibilityTree::endFrame(ImGuiID focusedItem) {
    if (focusedItem == 0) {
        return;
    }
    for (std::size_t index = 0; index < count; ++index) {
        if (targets[index] == focusedItem) {
            elements[index].states |= Element::Focused;
            focused = elements[index].id;
            return;
        }
    }
}

void AccessibilityTree::appendNumber(std::string& text, double value, int decimals) {
    std::array<char, 64> digits{};
    const std::to_chars_result written = std::to_chars(digits.data(), digits.data() + digits.size(), value, std::chars_format::fixed, decimals);
    text.append(digits.data(), written.ptr);
}

void AccessibilityTree::appendParts(std::string& text, std::initializer_list<std::string_view> parts) {
    const std::size_t start = text.size();
    for (const std::string_view part : parts) {
        if (part.empty()) {
            continue;
        }
        if (text.size() > start) {
            text.append(", ");
        }
        text.append(part);
    }
}

std::optional<ImGuiID> AccessibilityTree::findTarget(std::uint64_t element) const noexcept {
    for (std::size_t index = 0; index < count; ++index) {
        if (elements[index].id == element && targets[index] != 0) {
            return targets[index];
        }
    }
    return std::nullopt;
}

Element& AccessibilityTree::add(std::uint64_t id, const math::Rect& bounds) {
    if (count == elements.size()) {
        elements.emplace_back();
        targets.push_back(0);
    }
    Element& element = elements[count];
    element.id = id;
    element.parent = getParent();
    element.bounds = bounds;
    element.role = Element::Role::Group;
    element.label.clear();
    element.value.clear();
    element.hint.clear();
    element.states = 0;
    element.actions = 0;
    targets[count] = 0;
    ++count;
    return element;
}

std::uint64_t AccessibilityTree::getParent() const noexcept {
    for (auto level = levels.rbegin(); level != levels.rend(); ++level) {
        if (level->element) {
            return elements[*level->element].id;
        }
    }
    return 0;
}

} // namespace haylen::ui
