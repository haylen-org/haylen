#include "platform/windows/WindowsAccessibilityElement.hpp"

#include <oleauto.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <string>
#include <vector>

#include "platform/windows/WindowsAccessibilityRoot.hpp"
#include "platform/windows/WindowsAccessibilityTree.hpp"

namespace haylen::platform {

WindowsAccessibilityElement::WindowsAccessibilityElement(std::weak_ptr<WindowsAccessibilityTree> owner, std::uint64_t element) noexcept : tree(std::move(owner)), id(element) {}

IFACEMETHODIMP WindowsAccessibilityElement::QueryInterface(REFIID interfaceId, void** object) {
    if (interfaceId == __uuidof(IUnknown) || interfaceId == __uuidof(IRawElementProviderSimple)) {
        *object = static_cast<IRawElementProviderSimple*>(this);
    } else if (interfaceId == __uuidof(IRawElementProviderFragment)) {
        *object = static_cast<IRawElementProviderFragment*>(this);
    } else if (interfaceId == __uuidof(IInvokeProvider)) {
        *object = static_cast<IInvokeProvider*>(this);
    } else if (interfaceId == __uuidof(IToggleProvider)) {
        *object = static_cast<IToggleProvider*>(this);
    } else if (interfaceId == __uuidof(IRangeValueProvider)) {
        *object = static_cast<IRangeValueProvider*>(this);
    } else if (interfaceId == __uuidof(IValueProvider)) {
        *object = static_cast<IValueProvider*>(this);
    } else if (interfaceId == __uuidof(ISelectionItemProvider)) {
        *object = static_cast<ISelectionItemProvider*>(this);
    } else if (interfaceId == __uuidof(IExpandCollapseProvider)) {
        *object = static_cast<IExpandCollapseProvider*>(this);
    } else {
        *object = nullptr;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

IFACEMETHODIMP_(ULONG) WindowsAccessibilityElement::AddRef() {
    return ++references;
}

IFACEMETHODIMP_(ULONG) WindowsAccessibilityElement::Release() {
    const ULONG left = --references;
    if (left == 0) {
        delete this;
    }
    return left;
}

IFACEMETHODIMP WindowsAccessibilityElement::get_ProviderOptions(ProviderOptions* options) {
    *options = static_cast<ProviderOptions>(ProviderOptions_ServerSideProvider | ProviderOptions_UseComThreading);
    return S_OK;
}

IFACEMETHODIMP WindowsAccessibilityElement::GetPatternProvider(PATTERNID pattern, IUnknown** provider) {
    *provider = nullptr;
    const std::optional<AccessibilityElement> element = read();
    if (!element) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    if (hasPattern(*element, pattern)) {
        *provider = static_cast<IRawElementProviderSimple*>(this);
        AddRef();
    }
    return S_OK;
}

IFACEMETHODIMP WindowsAccessibilityElement::GetPropertyValue(PROPERTYID property, VARIANT* value) {
    value->vt = VT_EMPTY;
    const std::optional<AccessibilityElement> element = read();
    if (!element) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    // clang-format off
    const auto text = [value](const std::string& source) {
        value->vt = VT_BSTR;
        value->bstrVal = SysAllocString(WindowsAccessibilityTree::toWide(source).c_str());
    };
    const auto flag = [value](bool on) {
        value->vt = VT_BOOL;
        value->boolVal = on ? VARIANT_TRUE : VARIANT_FALSE;
    };
    // clang-format on
    switch (property) {
    case UIA_NamePropertyId:
        text(element->label);
        break;
    case UIA_HelpTextPropertyId:
        text(element->hint);
        break;
    case UIA_AutomationIdPropertyId:
        text(std::to_string(element->id));
        break;
    case UIA_FrameworkIdPropertyId:
        value->vt = VT_BSTR;
        value->bstrVal = SysAllocString(L"Haylen");
        break;
    case UIA_ControlTypePropertyId:
        value->vt = VT_I4;
        value->lVal = getControlType(element->role);
        break;
    case UIA_IsEnabledPropertyId:
        flag(!element->has(AccessibilityElement::Disabled));
        break;
    case UIA_IsKeyboardFocusablePropertyId:
        flag(element->can(AccessibilityElement::Focus));
        break;
    case UIA_HasKeyboardFocusPropertyId: {
        const std::shared_ptr<WindowsAccessibilityTree> owner = lockTree();
        flag(owner != nullptr && owner->getFocused() == id);
        break;
    }
    case UIA_IsControlElementPropertyId:
    case UIA_IsContentElementPropertyId:
        flag(true);
        break;
    case UIA_IsDialogPropertyId:
        flag(element->role == AccessibilityElement::Role::Dialog);
        break;
    case UIA_HeadingLevelPropertyId:
        value->vt = VT_I4;
        value->lVal = element->role == AccessibilityElement::Role::Heading ? HeadingLevel1 : HeadingLevel_None;
        break;
    case UIA_LiveSettingPropertyId:
        value->vt = VT_I4;
        value->lVal = element->role == AccessibilityElement::Role::Alert ? Assertive : Off;
        break;
    case UIA_ItemStatusPropertyId:
        if (element->has(AccessibilityElement::Busy)) {
            value->vt = VT_BSTR;
            value->bstrVal = SysAllocString(L"Busy");
        }
        break;
    default:
        break;
    }
    return S_OK;
}

IFACEMETHODIMP WindowsAccessibilityElement::get_HostRawElementProvider(IRawElementProviderSimple** provider) {
    *provider = nullptr;
    return S_OK;
}

// The parent of an element at the top is the root, and the siblings of an element are the children of its parent in their order.
IFACEMETHODIMP WindowsAccessibilityElement::Navigate(NavigateDirection direction, IRawElementProviderFragment** found) {
    *found = nullptr;
    const std::shared_ptr<WindowsAccessibilityTree> owner = lockTree();
    const std::optional<AccessibilityElement> element = owner != nullptr ? owner->find(id) : std::nullopt;
    if (!element) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    switch (direction) {
    case NavigateDirection_Parent:
        if (element->parent == 0 || !owner->find(element->parent)) {
            *found = owner->getRoot().Detach();
            return S_OK;
        }
        return returnFragment(element->parent, found);
    case NavigateDirection_FirstChild:
    case NavigateDirection_LastChild: {
        const std::vector<std::uint64_t> descendants = owner->getChildren(id);
        if (descendants.empty()) {
            return S_OK;
        }
        return returnFragment(direction == NavigateDirection_FirstChild ? descendants.front() : descendants.back(), found);
    }
    case NavigateDirection_NextSibling:
    case NavigateDirection_PreviousSibling: {
        const std::vector<std::uint64_t> siblings = owner->getChildren(owner->find(element->parent) ? element->parent : 0);
        const auto position = std::ranges::find(siblings, id);
        if (position == siblings.end()) {
            return S_OK;
        }
        if (direction == NavigateDirection_NextSibling) {
            return position + 1 != siblings.end() ? returnFragment(*(position + 1), found) : S_OK;
        }
        return position != siblings.begin() ? returnFragment(*(position - 1), found) : S_OK;
    }
    }
    return S_OK;
}

// The runtime id joins the mark of a fragment with the id of the element in two halves of 32 bits.
IFACEMETHODIMP WindowsAccessibilityElement::GetRuntimeId(SAFEARRAY** runtimeId) {
    *runtimeId = nullptr;
    const std::array<int, 3> parts{UiaAppendRuntimeId, static_cast<int>(id & 0xFFFFFFFFU), static_cast<int>(id >> 32U)};
    SAFEARRAY* array = SafeArrayCreateVector(VT_I4, 0, static_cast<ULONG>(parts.size()));
    if (array == nullptr) {
        return E_OUTOFMEMORY;
    }
    for (LONG index = 0; index < static_cast<LONG>(parts.size()); ++index) {
        int part = parts[static_cast<std::size_t>(index)];
        SafeArrayPutElement(array, &index, &part);
    }
    *runtimeId = array;
    return S_OK;
}

IFACEMETHODIMP WindowsAccessibilityElement::get_BoundingRectangle(UiaRect* rectangle) {
    *rectangle = {};
    const std::shared_ptr<WindowsAccessibilityTree> owner = lockTree();
    const std::optional<AccessibilityElement> element = owner != nullptr ? owner->find(id) : std::nullopt;
    if (!element) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    const math::Rect screen = owner->toScreen(element->bounds);
    *rectangle = {.left = screen.x, .top = screen.y, .width = screen.width, .height = screen.height};
    return S_OK;
}

IFACEMETHODIMP WindowsAccessibilityElement::GetEmbeddedFragmentRoots(SAFEARRAY** roots) {
    *roots = nullptr;
    return S_OK;
}

IFACEMETHODIMP WindowsAccessibilityElement::SetFocus() {
    return perform(AccessibilityElement::Focus);
}

IFACEMETHODIMP WindowsAccessibilityElement::get_FragmentRoot(IRawElementProviderFragmentRoot** found) {
    *found = nullptr;
    const std::shared_ptr<WindowsAccessibilityTree> owner = lockTree();
    if (owner == nullptr) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    *found = owner->getRoot().Detach();
    return S_OK;
}

IFACEMETHODIMP WindowsAccessibilityElement::Invoke() {
    return perform(AccessibilityElement::Activate);
}

IFACEMETHODIMP WindowsAccessibilityElement::Toggle() {
    return perform(AccessibilityElement::Activate);
}

IFACEMETHODIMP WindowsAccessibilityElement::get_ToggleState(ToggleState* state) {
    const std::optional<AccessibilityElement> element = read();
    *state = element && element->has(AccessibilityElement::Checked) ? ToggleState_On : ToggleState_Off;
    return element ? S_OK : UIA_E_ELEMENTNOTAVAILABLE;
}

// The engine knows the number of a value and not its range, so a new value moves the control one step toward it.
IFACEMETHODIMP WindowsAccessibilityElement::SetValue(double value) {
    const std::optional<AccessibilityElement> element = read();
    if (!element) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    const double current = readNumber(element->value);
    if (value > current) {
        return perform(AccessibilityElement::Increment);
    }
    if (value < current) {
        return perform(AccessibilityElement::Decrement);
    }
    return S_OK;
}

IFACEMETHODIMP WindowsAccessibilityElement::get_Value(double* value) {
    const std::optional<AccessibilityElement> element = read();
    *value = element ? readNumber(element->value) : 0.0;
    return element ? S_OK : UIA_E_ELEMENTNOTAVAILABLE;
}

// A range moves while the control takes steps, and the text of a field changes through the native field that edits it, never through UI Automation.
IFACEMETHODIMP WindowsAccessibilityElement::get_IsReadOnly(BOOL* readOnly) {
    const std::optional<AccessibilityElement> element = read();
    *readOnly = element && hasRange(*element) && element->can(AccessibilityElement::Increment) ? FALSE : TRUE;
    return element ? S_OK : UIA_E_ELEMENTNOTAVAILABLE;
}

IFACEMETHODIMP WindowsAccessibilityElement::get_Maximum(double* value) {
    return get_Value(value);
}

IFACEMETHODIMP WindowsAccessibilityElement::get_Minimum(double* value) {
    return get_Value(value);
}

IFACEMETHODIMP WindowsAccessibilityElement::get_LargeChange(double* value) {
    *value = 0.0;
    return S_OK;
}

IFACEMETHODIMP WindowsAccessibilityElement::get_SmallChange(double* value) {
    *value = 0.0;
    return S_OK;
}

IFACEMETHODIMP WindowsAccessibilityElement::SetValue(LPCWSTR) {
    return UIA_E_INVALIDOPERATION;
}

IFACEMETHODIMP WindowsAccessibilityElement::get_Value(BSTR* value) {
    const std::optional<AccessibilityElement> element = read();
    *value = SysAllocString(element ? WindowsAccessibilityTree::toWide(element->value).c_str() : L"");
    return element ? S_OK : UIA_E_ELEMENTNOTAVAILABLE;
}

IFACEMETHODIMP WindowsAccessibilityElement::Select() {
    return perform(AccessibilityElement::Activate);
}

IFACEMETHODIMP WindowsAccessibilityElement::AddToSelection() {
    return perform(AccessibilityElement::Activate);
}

IFACEMETHODIMP WindowsAccessibilityElement::RemoveFromSelection() {
    return UIA_E_INVALIDOPERATION;
}

IFACEMETHODIMP WindowsAccessibilityElement::get_IsSelected(BOOL* selected) {
    const std::optional<AccessibilityElement> element = read();
    *selected = element && element->has(AccessibilityElement::Selected) ? TRUE : FALSE;
    return element ? S_OK : UIA_E_ELEMENTNOTAVAILABLE;
}

IFACEMETHODIMP WindowsAccessibilityElement::get_SelectionContainer(IRawElementProviderSimple** container) {
    *container = nullptr;
    const std::shared_ptr<WindowsAccessibilityTree> owner = lockTree();
    const std::optional<AccessibilityElement> element = owner != nullptr ? owner->find(id) : std::nullopt;
    if (element && owner->find(element->parent)) {
        *container = owner->getProvider(element->parent).Detach();
    }
    return S_OK;
}

IFACEMETHODIMP WindowsAccessibilityElement::Expand() {
    const std::optional<AccessibilityElement> element = read();
    return element && !element->has(AccessibilityElement::Expanded) ? perform(AccessibilityElement::Activate) : S_OK;
}

IFACEMETHODIMP WindowsAccessibilityElement::Collapse() {
    const std::optional<AccessibilityElement> element = read();
    return element && element->has(AccessibilityElement::Expanded) ? perform(AccessibilityElement::Activate) : S_OK;
}

IFACEMETHODIMP WindowsAccessibilityElement::get_ExpandCollapseState(ExpandCollapseState* state) {
    const std::optional<AccessibilityElement> element = read();
    *state = element && element->has(AccessibilityElement::Expanded) ? ExpandCollapseState_Expanded : ExpandCollapseState_Collapsed;
    return element ? S_OK : UIA_E_ELEMENTNOTAVAILABLE;
}

CONTROLTYPEID WindowsAccessibilityElement::getControlType(AccessibilityElement::Role role) noexcept {
    using Role = AccessibilityElement::Role;
    switch (role) {
    case Role::Group:
    case Role::RadioGroup:
        return UIA_GroupControlTypeId;
    case Role::Text:
    case Role::Heading:
    case Role::Alert:
        return UIA_TextControlTypeId;
    case Role::Image:
        return UIA_ImageControlTypeId;
    case Role::Button:
    case Role::Switch:
        return UIA_ButtonControlTypeId;
    case Role::CheckBox:
        return UIA_CheckBoxControlTypeId;
    case Role::RadioButton:
        return UIA_RadioButtonControlTypeId;
    case Role::TabList:
        return UIA_TabControlTypeId;
    case Role::Tab:
        return UIA_TabItemControlTypeId;
    case Role::Slider:
        return UIA_SliderControlTypeId;
    case Role::TextField:
        return UIA_EditControlTypeId;
    case Role::ComboBox:
        return UIA_ComboBoxControlTypeId;
    case Role::ProgressBar:
        return UIA_ProgressBarControlTypeId;
    case Role::List:
        return UIA_ListControlTypeId;
    case Role::Dialog:
    case Role::PlayArea:
        return UIA_PaneControlTypeId;
    }
    return UIA_CustomControlTypeId;
}

bool WindowsAccessibilityElement::hasPattern(const AccessibilityElement& element, PATTERNID pattern) noexcept {
    using Role = AccessibilityElement::Role;
    switch (pattern) {
    case UIA_InvokePatternId:
        return element.can(AccessibilityElement::Activate) && (element.role == Role::Button || element.role == Role::Image || element.role == Role::Text || element.role == Role::Group || element.role == Role::List);
    case UIA_TogglePatternId:
        return element.role == Role::CheckBox || element.role == Role::Switch;
    case UIA_RangeValuePatternId:
        return hasRange(element);
    case UIA_ValuePatternId:
        return element.role == Role::TextField || (!hasRange(element) && !element.value.empty() && element.role != Role::ComboBox);
    case UIA_SelectionItemPatternId:
        return element.role == Role::Tab || element.role == Role::RadioButton;
    case UIA_ExpandCollapsePatternId:
        return element.role == Role::ComboBox;
    default:
        return false;
    }
}

bool WindowsAccessibilityElement::hasRange(const AccessibilityElement& element) noexcept {
    return element.role == AccessibilityElement::Role::Slider || element.role == AccessibilityElement::Role::ProgressBar;
}

double WindowsAccessibilityElement::readNumber(const std::string& text) noexcept {
    double number = 0.0;
    std::from_chars(text.data(), text.data() + text.size(), number);
    return number;
}

std::shared_ptr<WindowsAccessibilityTree> WindowsAccessibilityElement::lockTree() const {
    return tree.lock();
}

std::optional<AccessibilityElement> WindowsAccessibilityElement::read() const {
    const std::shared_ptr<WindowsAccessibilityTree> owner = lockTree();
    return owner != nullptr ? owner->find(id) : std::nullopt;
}

HRESULT WindowsAccessibilityElement::perform(AccessibilityElement::Action action) const {
    const std::shared_ptr<WindowsAccessibilityTree> owner = lockTree();
    const std::optional<AccessibilityElement> element = owner != nullptr ? owner->find(id) : std::nullopt;
    if (!element) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    if (element->has(AccessibilityElement::Disabled)) {
        return UIA_E_ELEMENTNOTENABLED;
    }
    if (!element->can(action)) {
        return UIA_E_INVALIDOPERATION;
    }
    owner->perform(id, action);
    return S_OK;
}

HRESULT WindowsAccessibilityElement::returnFragment(std::uint64_t element, IRawElementProviderFragment** found) const {
    const std::shared_ptr<WindowsAccessibilityTree> owner = lockTree();
    if (owner == nullptr) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    *found = owner->getProvider(element).Detach();
    return S_OK;
}

} // namespace haylen::platform
