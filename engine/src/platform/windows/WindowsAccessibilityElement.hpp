#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <ole2.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <uiautomation.h>

#include "haylen/platform/AccessibilityElement.hpp"

namespace haylen::platform {

class WindowsAccessibilityTree;

// The provider of UI Automation for one element of the tree, with the patterns of its kind: buttons invoke, check boxes and switches toggle, sliders and progress bars give a range value, fields give the text of their value, tabs and radio buttons are selection items and combo boxes expand and collapse. It reads the element from the tree at every call, and an element that left the tree answers that it is no longer available.
class WindowsAccessibilityElement final : public IRawElementProviderSimple, public IRawElementProviderFragment, public IInvokeProvider, public IToggleProvider, public IRangeValueProvider, public IValueProvider, public ISelectionItemProvider, public IExpandCollapseProvider {
  public:
    WindowsAccessibilityElement(std::weak_ptr<WindowsAccessibilityTree> owner, std::uint64_t element) noexcept;

    WindowsAccessibilityElement(const WindowsAccessibilityElement&) = delete;
    WindowsAccessibilityElement& operator=(const WindowsAccessibilityElement&) = delete;

    IFACEMETHODIMP QueryInterface(REFIID interfaceId, void** object) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    [[nodiscard]] std::uint64_t getId() const noexcept {
        return id;
    }

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

    IFACEMETHODIMP Invoke() override;

    IFACEMETHODIMP Toggle() override;
    IFACEMETHODIMP get_ToggleState(ToggleState* state) override;

    IFACEMETHODIMP SetValue(double value) override;
    IFACEMETHODIMP get_Value(double* value) override;
    IFACEMETHODIMP get_IsReadOnly(BOOL* readOnly) override;
    IFACEMETHODIMP get_Maximum(double* value) override;
    IFACEMETHODIMP get_Minimum(double* value) override;
    IFACEMETHODIMP get_LargeChange(double* value) override;
    IFACEMETHODIMP get_SmallChange(double* value) override;

    IFACEMETHODIMP SetValue(LPCWSTR value) override;
    IFACEMETHODIMP get_Value(BSTR* value) override;

    IFACEMETHODIMP Select() override;
    IFACEMETHODIMP AddToSelection() override;
    IFACEMETHODIMP RemoveFromSelection() override;
    IFACEMETHODIMP get_IsSelected(BOOL* selected) override;
    IFACEMETHODIMP get_SelectionContainer(IRawElementProviderSimple** container) override;

    IFACEMETHODIMP Expand() override;
    IFACEMETHODIMP Collapse() override;
    IFACEMETHODIMP get_ExpandCollapseState(ExpandCollapseState* state) override;

    // The control type of UI Automation for a role, and whether the role takes a pattern.
    [[nodiscard]] static CONTROLTYPEID getControlType(AccessibilityElement::Role role) noexcept;
    [[nodiscard]] static bool hasPattern(const AccessibilityElement& element, PATTERNID pattern) noexcept;
    [[nodiscard]] static bool hasRange(const AccessibilityElement& element) noexcept;
    [[nodiscard]] static double readNumber(const std::string& text) noexcept;

  private:
    [[nodiscard]] std::shared_ptr<WindowsAccessibilityTree> lockTree() const;
    [[nodiscard]] std::optional<AccessibilityElement> read() const;
    [[nodiscard]] HRESULT perform(AccessibilityElement::Action action) const;
    [[nodiscard]] HRESULT returnFragment(std::uint64_t element, IRawElementProviderFragment** found) const;

    ~WindowsAccessibilityElement() = default;

    std::atomic<ULONG> references = 1;
    std::weak_ptr<WindowsAccessibilityTree> tree;
    std::uint64_t id;
};

} // namespace haylen::platform
