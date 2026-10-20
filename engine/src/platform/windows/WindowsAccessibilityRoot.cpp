#include "platform/windows/WindowsAccessibilityRoot.hpp"

#include <oleauto.h>

#include <vector>

#include "platform/windows/WindowsAccessibilityElement.hpp"
#include "platform/windows/WindowsAccessibilityTree.hpp"

namespace haylen::platform {

WindowsAccessibilityRoot::WindowsAccessibilityRoot(std::weak_ptr<WindowsAccessibilityTree> owner) noexcept : tree(std::move(owner)) {}

IFACEMETHODIMP WindowsAccessibilityRoot::QueryInterface(REFIID interfaceId, void** object) {
    if (interfaceId == __uuidof(IUnknown) || interfaceId == __uuidof(IRawElementProviderSimple)) {
        *object = static_cast<IRawElementProviderSimple*>(this);
    } else if (interfaceId == __uuidof(IRawElementProviderFragment)) {
        *object = static_cast<IRawElementProviderFragment*>(this);
    } else if (interfaceId == __uuidof(IRawElementProviderFragmentRoot)) {
        *object = static_cast<IRawElementProviderFragmentRoot*>(this);
    } else {
        *object = nullptr;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

IFACEMETHODIMP_(ULONG) WindowsAccessibilityRoot::AddRef() {
    return ++references;
}

IFACEMETHODIMP_(ULONG) WindowsAccessibilityRoot::Release() {
    const ULONG left = --references;
    if (left == 0) {
        delete this;
    }
    return left;
}

IFACEMETHODIMP WindowsAccessibilityRoot::get_ProviderOptions(ProviderOptions* options) {
    *options = static_cast<ProviderOptions>(ProviderOptions_ServerSideProvider | ProviderOptions_UseComThreading);
    return S_OK;
}

IFACEMETHODIMP WindowsAccessibilityRoot::GetPatternProvider(PATTERNID, IUnknown** provider) {
    *provider = nullptr;
    return S_OK;
}

// The provider of the window gives the name, the type and the place of the root, which only adds the name of the engine.
IFACEMETHODIMP WindowsAccessibilityRoot::GetPropertyValue(PROPERTYID property, VARIANT* value) {
    value->vt = VT_EMPTY;
    if (property == UIA_FrameworkIdPropertyId) {
        value->vt = VT_BSTR;
        value->bstrVal = SysAllocString(L"Haylen");
    }
    return S_OK;
}

IFACEMETHODIMP WindowsAccessibilityRoot::get_HostRawElementProvider(IRawElementProviderSimple** provider) {
    *provider = nullptr;
    const std::shared_ptr<WindowsAccessibilityTree> owner = tree.lock();
    if (owner == nullptr || owner->getWindow() == nullptr) {
        return S_OK;
    }
    return UiaHostProviderFromHwnd(owner->getWindow(), provider);
}

IFACEMETHODIMP WindowsAccessibilityRoot::Navigate(NavigateDirection direction, IRawElementProviderFragment** found) {
    *found = nullptr;
    const std::shared_ptr<WindowsAccessibilityTree> owner = tree.lock();
    if (owner == nullptr || (direction != NavigateDirection_FirstChild && direction != NavigateDirection_LastChild)) {
        return S_OK;
    }
    const std::vector<std::uint64_t> top = owner->getChildren(0);
    if (!top.empty()) {
        *found = owner->getProvider(direction == NavigateDirection_FirstChild ? top.front() : top.back()).Detach();
    }
    return S_OK;
}

IFACEMETHODIMP WindowsAccessibilityRoot::GetRuntimeId(SAFEARRAY** runtimeId) {
    *runtimeId = nullptr;
    return S_OK;
}

IFACEMETHODIMP WindowsAccessibilityRoot::get_BoundingRectangle(UiaRect* rectangle) {
    *rectangle = {};
    return S_OK;
}

IFACEMETHODIMP WindowsAccessibilityRoot::GetEmbeddedFragmentRoots(SAFEARRAY** roots) {
    *roots = nullptr;
    return S_OK;
}

IFACEMETHODIMP WindowsAccessibilityRoot::SetFocus() {
    return S_OK;
}

IFACEMETHODIMP WindowsAccessibilityRoot::get_FragmentRoot(IRawElementProviderFragmentRoot** found) {
    *found = this;
    AddRef();
    return S_OK;
}

IFACEMETHODIMP WindowsAccessibilityRoot::ElementProviderFromPoint(double x, double y, IRawElementProviderFragment** found) {
    *found = nullptr;
    const std::shared_ptr<WindowsAccessibilityTree> owner = tree.lock();
    if (owner == nullptr) {
        return S_OK;
    }
    const std::uint64_t element = owner->findAt({static_cast<float>(x), static_cast<float>(y)});
    if (element != 0) {
        *found = owner->getProvider(element).Detach();
    }
    return S_OK;
}

IFACEMETHODIMP WindowsAccessibilityRoot::GetFocus(IRawElementProviderFragment** found) {
    *found = nullptr;
    const std::shared_ptr<WindowsAccessibilityTree> owner = tree.lock();
    const std::uint64_t focused = owner != nullptr ? owner->getFocused() : 0;
    if (focused != 0) {
        *found = owner->getProvider(focused).Detach();
    }
    return S_OK;
}

} // namespace haylen::platform
