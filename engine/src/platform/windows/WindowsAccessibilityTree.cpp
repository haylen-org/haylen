#include "platform/windows/WindowsAccessibilityTree.hpp"

#include <oleauto.h>

#include <algorithm>
#include <utility>

#include "platform/windows/WindowsAccessibilityElement.hpp"
#include "platform/windows/WindowsAccessibilityRoot.hpp"

namespace haylen::platform {

WindowsAccessibilityTree::WindowsAccessibilityTree(HWND owner, Performer action) : window(owner), performer(std::move(action)) {}

WindowsAccessibilityTree::~WindowsAccessibilityTree() = default;

void WindowsAccessibilityTree::update(std::span<const AccessibilityElement> tree, std::uint64_t focusedElement, math::Vec2 framebuffer) {
    std::vector<Change> changes;
    bool structureChanged = false;
    bool focusChanged = false;
    {
        const std::scoped_lock lock(mutex);
        std::vector<AccessibilityElement> before = std::move(elements);
        const std::unordered_map<std::uint64_t, std::size_t> indicesBefore = std::move(indices);
        const std::unordered_map<std::uint64_t, std::vector<std::uint64_t>> childrenBefore = std::move(children);

        elements.assign(tree.begin(), tree.end());
        indices.clear();
        children.clear();
        for (std::size_t index = 0; index < elements.size(); ++index) {
            indices.emplace(elements[index].id, index);
            children[elements[index].parent].push_back(elements[index].id);
        }
        structureChanged = children != childrenBefore;
        focusChanged = focusedElement != focused;
        focused = focusedElement;

        // The framebuffer covers the client area of the window, which UI Automation measures in pixels of the screen.
        RECT client{};
        if (window != nullptr && GetClientRect(window, &client) != FALSE && framebuffer.x > 0.0F) {
            scale = static_cast<float>(client.right) / framebuffer.x;
        }

        for (const AccessibilityElement& element : elements) {
            if (const auto found = indicesBefore.find(element.id); found != indicesBefore.end()) {
                std::vector<Change> elementChanges = compare(before[found->second], element);
                changes.insert(changes.end(), elementChanges.begin(), elementChanges.end());
            }
        }

        // The providers of the elements that left the tree go, and clients that still hold one hear that it is gone.
        std::erase_if(providers, [this](const auto& entry) { return !indices.contains(entry.first); });
    }

    if (UiaClientsAreListening() == FALSE) {
        return;
    }
    if (structureChanged) {
        UiaRaiseStructureChangedEvent(getRoot().Get(), StructureChangeType_ChildrenInvalidated, nullptr, 0);
    }
    for (const Change& change : changes) {
        raise(change);
    }
    if (focusChanged && find(focusedElement)) {
        UiaRaiseAutomationEvent(getProvider(focusedElement).Get(), UIA_AutomationFocusChangedEventId);
    }
}

void WindowsAccessibilityTree::announce(std::string_view text, bool interrupt) {
    if (UiaClientsAreListening() == FALSE) {
        return;
    }
    const std::wstring wide = toWide(text);
    BSTR displayString = SysAllocString(wide.c_str());
    BSTR activityId = SysAllocString(L"haylen.announcement");
    UiaRaiseNotificationEvent(getRoot().Get(), NotificationKind_Other, interrupt ? NotificationProcessing_ImportantMostRecent : NotificationProcessing_All, displayString, activityId);
    SysFreeString(activityId);
    SysFreeString(displayString);
}

void WindowsAccessibilityTree::perform(std::uint64_t element, AccessibilityElement::Action action) const {
    performer({.element = element, .action = action});
}

Microsoft::WRL::ComPtr<WindowsAccessibilityRoot> WindowsAccessibilityTree::getRoot() {
    const std::scoped_lock lock(mutex);
    if (!root) {
        root.Attach(new WindowsAccessibilityRoot(weak_from_this()));
    }
    return root;
}

Microsoft::WRL::ComPtr<WindowsAccessibilityElement> WindowsAccessibilityTree::getProvider(std::uint64_t id) {
    const std::scoped_lock lock(mutex);
    Microsoft::WRL::ComPtr<WindowsAccessibilityElement>& provider = providers[id];
    if (!provider) {
        provider.Attach(new WindowsAccessibilityElement(weak_from_this(), id));
    }
    return provider;
}

std::optional<AccessibilityElement> WindowsAccessibilityTree::find(std::uint64_t id) const {
    const std::scoped_lock lock(mutex);
    const auto found = indices.find(id);
    if (found == indices.end()) {
        return std::nullopt;
    }
    return elements[found->second];
}

std::vector<std::uint64_t> WindowsAccessibilityTree::getChildren(std::uint64_t parent) const {
    const std::scoped_lock lock(mutex);
    const auto found = children.find(parent);
    return found != children.end() ? found->second : std::vector<std::uint64_t>{};
}

std::uint64_t WindowsAccessibilityTree::getFocused() const {
    const std::scoped_lock lock(mutex);
    return indices.contains(focused) ? focused : 0;
}

std::uint64_t WindowsAccessibilityTree::findAt(math::Vec2 point) const {
    const std::scoped_lock lock(mutex);
    return findAtLocked(0, point);
}

// Later siblings lie above earlier ones, so the search goes from the last child back.
std::uint64_t WindowsAccessibilityTree::findAtLocked(std::uint64_t parent, math::Vec2 point) const {
    const auto found = children.find(parent);
    if (found == children.end()) {
        return parent;
    }
    for (auto child = found->second.rbegin(); child != found->second.rend(); ++child) {
        if (toScreenLocked(elements[indices.at(*child)].bounds).contains(point)) {
            return findAtLocked(*child, point);
        }
    }
    return parent;
}

math::Rect WindowsAccessibilityTree::toScreen(const math::Rect& bounds) const {
    const std::scoped_lock lock(mutex);
    return toScreenLocked(bounds);
}

math::Rect WindowsAccessibilityTree::toScreenLocked(const math::Rect& bounds) const {
    POINT origin{};
    if (window != nullptr) {
        ClientToScreen(window, &origin);
    }
    return {static_cast<float>(origin.x) + bounds.x * scale, static_cast<float>(origin.y) + bounds.y * scale, bounds.width * scale, bounds.height * scale};
}

void WindowsAccessibilityTree::disconnect() {
    std::unordered_map<std::uint64_t, Microsoft::WRL::ComPtr<WindowsAccessibilityElement>> released;
    {
        const std::scoped_lock lock(mutex);
        released = std::move(providers);
        providers.clear();
        elements.clear();
        indices.clear();
        children.clear();
    }
    for (const auto& [id, provider] : released) {
        UiaDisconnectProvider(provider.Get());
    }
}

std::wstring WindowsAccessibilityTree::toWide(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);
    return wide;
}

// The name, the value and the states that screen readers follow, each as the property of UI Automation that carries it.
std::vector<WindowsAccessibilityTree::Change> WindowsAccessibilityTree::compare(const AccessibilityElement& before, const AccessibilityElement& after) {
    std::vector<Change> changes;
    if (before.label != after.label) {
        changes.push_back({.id = after.id, .property = UIA_NamePropertyId, .before = toWide(before.label), .after = toWide(after.label)});
    }
    if (before.value != after.value && WindowsAccessibilityElement::hasPattern(after, UIA_ValuePatternId)) {
        changes.push_back({.id = after.id, .property = UIA_ValueValuePropertyId, .before = toWide(before.value), .after = toWide(after.value)});
    }
    if (before.value != after.value && WindowsAccessibilityElement::hasRange(after)) {
        changes.push_back({.id = after.id, .property = UIA_RangeValueValuePropertyId, .numberBefore = WindowsAccessibilityElement::readNumber(before.value), .numberAfter = WindowsAccessibilityElement::readNumber(after.value)});
    }
    if (before.has(AccessibilityElement::Checked) != after.has(AccessibilityElement::Checked) && WindowsAccessibilityElement::hasPattern(after, UIA_TogglePatternId)) {
        changes.push_back({.id = after.id, .property = UIA_ToggleToggleStatePropertyId, .stateBefore = before.has(AccessibilityElement::Checked) ? ToggleState_On : ToggleState_Off, .stateAfter = after.has(AccessibilityElement::Checked) ? ToggleState_On : ToggleState_Off});
    }
    const bool selectedBefore = WindowsAccessibilityElement::isSelected(before);
    const bool selectedAfter = WindowsAccessibilityElement::isSelected(after);
    if (selectedBefore != selectedAfter && WindowsAccessibilityElement::hasPattern(after, UIA_SelectionItemPatternId)) {
        changes.push_back({.id = after.id, .property = UIA_SelectionItemIsSelectedPropertyId, .flagBefore = selectedBefore ? TRUE : FALSE, .flagAfter = selectedAfter ? TRUE : FALSE});
        // Screen readers say that an item was picked when it raises the event of its selection, beside the property that changed.
        if (selectedAfter) {
            changes.push_back({.id = after.id, .event = UIA_SelectionItem_ElementSelectedEventId});
        }
    }
    if (before.has(AccessibilityElement::Expanded) != after.has(AccessibilityElement::Expanded) && WindowsAccessibilityElement::hasPattern(after, UIA_ExpandCollapsePatternId)) {
        changes.push_back({.id = after.id, .property = UIA_ExpandCollapseExpandCollapseStatePropertyId, .stateBefore = before.has(AccessibilityElement::Expanded) ? ExpandCollapseState_Expanded : ExpandCollapseState_Collapsed, .stateAfter = after.has(AccessibilityElement::Expanded) ? ExpandCollapseState_Expanded : ExpandCollapseState_Collapsed});
    }
    if (before.has(AccessibilityElement::Disabled) != after.has(AccessibilityElement::Disabled)) {
        changes.push_back({.id = after.id, .property = UIA_IsEnabledPropertyId, .flagBefore = before.has(AccessibilityElement::Disabled) ? FALSE : TRUE, .flagAfter = after.has(AccessibilityElement::Disabled) ? FALSE : TRUE});
    }
    return changes;
}

void WindowsAccessibilityTree::raise(const Change& change) {
    if (change.event != 0) {
        UiaRaiseAutomationEvent(getProvider(change.id).Get(), change.event);
        return;
    }
    VARIANT before{};
    VARIANT after{};
    if (change.flagBefore) {
        before.vt = VT_BOOL;
        before.boolVal = *change.flagBefore != FALSE ? VARIANT_TRUE : VARIANT_FALSE;
        after.vt = VT_BOOL;
        after.boolVal = *change.flagAfter != FALSE ? VARIANT_TRUE : VARIANT_FALSE;
    } else if (change.stateBefore) {
        before.vt = VT_I4;
        before.lVal = *change.stateBefore;
        after.vt = VT_I4;
        after.lVal = *change.stateAfter;
    } else if (change.numberBefore) {
        before.vt = VT_R8;
        before.dblVal = *change.numberBefore;
        after.vt = VT_R8;
        after.dblVal = *change.numberAfter;
    } else {
        before.vt = VT_BSTR;
        before.bstrVal = SysAllocString(change.before.c_str());
        after.vt = VT_BSTR;
        after.bstrVal = SysAllocString(change.after.c_str());
    }
    UiaRaiseAutomationPropertyChangedEvent(getProvider(change.id).Get(), change.property, before, after);
    VariantClear(&before);
    VariantClear(&after);
}

} // namespace haylen::platform
