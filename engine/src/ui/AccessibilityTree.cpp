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
    Element& element = add(context, node, bounds);
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

void AccessibilityTree::addElement(const Context& context, std::uint64_t id, const math::Rect& bounds, Element::Role role, std::string_view label) {
    if (!enabled || (!levels.empty() && levels.back().hidden)) {
        return;
    }

    Element& element = add(context, id, bounds);
    element.role = role;
    element.label.assign(label);
}

void AccessibilityTree::addPart(const Context& context, ImGuiID target, const math::Rect& bounds, Element::Role role, std::string_view label, std::uint8_t states) {
    const std::size_t before = count;
    addElement(context, target, bounds, role, label);
    if (count == before) {
        return;
    }

    Element& element = elements[count - 1];
    element.states = states;
    if (target != 0 && (states & Element::Disabled) == 0) {
        element.actions = Element::Focus | Element::Activate;
        targets[count - 1] = target;
    }
}

void AccessibilityTree::addButton(const Context& context, ImGuiID item, const math::Rect& bounds, std::string_view label) {
    const std::size_t before = count;
    addElement(context, item, bounds, Element::Role::Button, label);
    if (count == before) {
        return;
    }

    elements[count - 1].actions = Element::Activate;
    buttons[count - 1] = item;
}

bool AccessibilityTree::press(std::uint64_t element) noexcept {
    for (std::size_t index = 0; index < count; ++index) {
        if (elements[index].id == element && buttons[index] != 0) {
            pressed = buttons[index];
            return true;
        }
    }

    return false;
}

bool AccessibilityTree::takePress(ImGuiID item) noexcept {
    if (pressed == 0 || pressed != item) {
        return false;
    }

    pressed = 0;
    return true;
}

void AccessibilityTree::beginHidden() {
    levels.push_back({.hidden = true});
}

void AccessibilityTree::endHidden() {
    levels.pop_back();
}

void AccessibilityTree::endFrame(ImGuiID focusedItem) {
    pressed = 0;
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

// The bounds clamp into the clip of the window, which keeps the part that shows and turns bounds wholly outside it into a line on its nearest edge. Nodes that the transforms of the window scale to nothing show nowhere, so their element has no area.
Element& AccessibilityTree::add(const Context& context, std::uint64_t id, const math::Rect& bounds) {
    if (count == elements.size()) {
        elements.emplace_back();
        targets.push_back(0);
        buttons.push_back(0);
    }

    const std::optional<math::Rect> clip = context.getClip();
    const math::Rect shown = clip ? math::Rect::fromMinMax(clip->clamp(bounds.getMin()), clip->clamp(bounds.getMax())) : math::Rect{bounds.getCenter().x, bounds.getCenter().y, 0.0F, 0.0F};

    Element& element = elements[count];
    element.id = id;
    element.parent = getParent();
    element.bounds = shown;
    element.role = Element::Role::Group;
    element.label.clear();
    element.value.clear();
    element.hint.clear();
    element.states = 0;
    element.actions = 0;
    targets[count] = 0;
    buttons[count] = 0;
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
