#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <ole2.h>

#include <atomic>
#include <memory>
#include <uiautomation.h>

namespace haylen::platform {

class WindowsAccessibilityTree;

// The provider of UI Automation for the window of the app, the root of its fragment of elements. The window itself names it and places it, through the provider that UI Automation keeps for every window, and the root lists the elements at the top, finds the element under a point and tells which one holds the focus.
class WindowsAccessibilityRoot final : public IRawElementProviderSimple, public IRawElementProviderFragment, public IRawElementProviderFragmentRoot {
  public:
    explicit WindowsAccessibilityRoot(std::weak_ptr<WindowsAccessibilityTree> owner) noexcept;

    WindowsAccessibilityRoot(const WindowsAccessibilityRoot&) = delete;
    WindowsAccessibilityRoot& operator=(const WindowsAccessibilityRoot&) = delete;

    IFACEMETHODIMP QueryInterface(REFIID interfaceId, void** object) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    IFACEMETHODIMP get_ProviderOptions(ProviderOptions* options) override;
    IFACEMETHODIMP GetPatternProvider(PATTERNID pattern, IUnknown** provider) override;
    IFACEMETHODIMP GetPropertyValue(PROPERTYID property, VARIANT* value) override;
    IFACEMETHODIMP get_HostRawElementProvider(IRawElementProviderSimple** provider) override;

    IFACEMETHODIMP Navigate(NavigateDirection direction, IRawElementProviderFragment** found) override;
    IFACEMETHODIMP GetRuntimeId(SAFEARRAY** runtimeId) override;
    IFACEMETHODIMP get_BoundingRectangle(UiaRect* rectangle) override;
    IFACEMETHODIMP GetEmbeddedFragmentRoots(SAFEARRAY** roots) override;
    IFACEMETHODIMP SetFocus() override;
    IFACEMETHODIMP get_FragmentRoot(IRawElementProviderFragmentRoot** found) override;

    IFACEMETHODIMP ElementProviderFromPoint(double x, double y, IRawElementProviderFragment** found) override;
    IFACEMETHODIMP GetFocus(IRawElementProviderFragment** found) override;

  private:
    ~WindowsAccessibilityRoot() = default;

    std::atomic<ULONG> references = 1;
    std::weak_ptr<WindowsAccessibilityTree> tree;
};

} // namespace haylen::platform
