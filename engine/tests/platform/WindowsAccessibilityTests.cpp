#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "platform/windows/WindowsAccessibilityElement.hpp"
#include "platform/windows/WindowsAccessibilityRoot.hpp"
#include "platform/windows/WindowsAccessibilityTree.hpp"

namespace haylen::platform {

// A tree of a group with a button, a check box and a slider, and a field at the top, as UI Automation reads it through its providers.
class WindowsAccessibilityTest : public ::testing::Test {
  protected:
    using Role = AccessibilityElement::Role;
    using Fragment = Microsoft::WRL::ComPtr<IRawElementProviderFragment>;

    WindowsAccessibilityTest() : tree(std::make_shared<WindowsAccessibilityTree>(nullptr, [this](const Accessibility::Request& request) { requests.push_back(request); })) {
        tree->update(makeElements(true), 2, {800.0F, 600.0F});
    }

    [[nodiscard]] static std::vector<AccessibilityElement> makeElements(bool withSlider) {
        const auto bits = [](int value) { return static_cast<std::uint8_t>(value); };
        std::vector<AccessibilityElement> elements{
            {.id = 1, .bounds = {0.0F, 0.0F, 400.0F, 300.0F}, .role = Role::Group, .label = "Settings"}, {.id = 2, .parent = 1, .bounds = {10.0F, 10.0F, 100.0F, 40.0F}, .role = Role::Button, .label = "Play", .actions = bits(AccessibilityElement::Focus | AccessibilityElement::Activate)}, {.id = 3, .parent = 1, .bounds = {10.0F, 60.0F, 100.0F, 40.0F}, .role = Role::CheckBox, .label = "Music", .states = bits(AccessibilityElement::Checked), .actions = bits(AccessibilityElement::Focus | AccessibilityElement::Activate)}, {.id = 4, .parent = 1, .bounds = {10.0F, 110.0F, 200.0F, 40.0F}, .role = Role::Slider, .label = "Volume", .value = "40", .actions = bits(AccessibilityElement::Focus | AccessibilityElement::Increment | AccessibilityElement::Decrement)}, {.id = 5, .bounds = {0.0F, 300.0F, 400.0F, 40.0F}, .role = Role::TextField, .label = "Name", .value = "Ana", .actions = bits(AccessibilityElement::Focus)},
        };

        if (!withSlider) {
            std::erase_if(elements, [](const AccessibilityElement& element) { return element.id == 4; });
        }

        return elements;
    }

    [[nodiscard]] static std::wstring readName(IUnknown* provider) {
        if (provider == nullptr) {
            throw std::runtime_error("The tree gave no provider whose name a test reads.");
        }

        Microsoft::WRL::ComPtr<IRawElementProviderSimple> simple;
        VARIANT value{};
        if (FAILED(provider->QueryInterface(IID_PPV_ARGS(&simple))) || FAILED(simple->GetPropertyValue(UIA_NamePropertyId, &value))) {
            throw std::runtime_error("A provider of the tree gave no name.");
        }

        std::wstring name = value.vt == VT_BSTR ? value.bstrVal : L"";
        VariantClear(&value);
        return name;
    }

    [[nodiscard]] static Fragment navigate(IRawElementProviderFragment* from, NavigateDirection direction) {
        Fragment found;
        if (FAILED(from->Navigate(direction, &found))) {
            throw std::runtime_error("A provider of the tree could not navigate.");
        }

        return found;
    }

    // The provider of an element that the tree holds.
    [[nodiscard]] Microsoft::WRL::ComPtr<WindowsAccessibilityElement> getElement(std::uint64_t id) {
        Microsoft::WRL::ComPtr<WindowsAccessibilityElement> element = tree->getProvider(id);
        if (!element) {
            throw std::runtime_error("The tree has no provider of the element " + std::to_string(id) + ".");
        }

        return element;
    }

    // The provider of a pattern of an element, or nothing for a pattern that the element lacks.
    template <typename Pattern> [[nodiscard]] Microsoft::WRL::ComPtr<Pattern> getPattern(std::uint64_t id, PATTERNID pattern) {
        Microsoft::WRL::ComPtr<IUnknown> provider;
        if (FAILED(getElement(id)->GetPatternProvider(pattern, &provider))) {
            throw std::runtime_error("The element " + std::to_string(id) + " gave no answer about a pattern.");
        }

        Microsoft::WRL::ComPtr<Pattern> found;
        if (provider && FAILED(provider.As(&found))) {
            throw std::runtime_error("The pattern of the element " + std::to_string(id) + " has another interface.");
        }

        return found;
    }

    std::vector<Accessibility::Request> requests;
    std::shared_ptr<WindowsAccessibilityTree> tree;
};

TEST_F(WindowsAccessibilityTest, NavigatesTheElementsFromTheRoot) {
    const Microsoft::WRL::ComPtr<WindowsAccessibilityRoot> root = tree->getRoot();
    const Fragment group = navigate(root.Get(), NavigateDirection_FirstChild);
    ASSERT_TRUE(group);
    EXPECT_EQ(readName(group.Get()), L"Settings");
    const Fragment play = navigate(group.Get(), NavigateDirection_FirstChild);
    ASSERT_TRUE(play);
    EXPECT_EQ(readName(play.Get()), L"Play");
    EXPECT_EQ(readName(navigate(play.Get(), NavigateDirection_NextSibling).Get()), L"Music");
    EXPECT_FALSE(navigate(play.Get(), NavigateDirection_PreviousSibling));
    EXPECT_EQ(readName(navigate(group.Get(), NavigateDirection_LastChild).Get()), L"Volume");
    EXPECT_EQ(readName(navigate(root.Get(), NavigateDirection_LastChild).Get()), L"Name");
    EXPECT_EQ(readName(navigate(play.Get(), NavigateDirection_Parent).Get()), L"Settings");

    // The parent of an element at the top is the root of the window.
    Microsoft::WRL::ComPtr<IUnknown> parent;
    ASSERT_HRESULT_SUCCEEDED(navigate(group.Get(), NavigateDirection_Parent).As(&parent));
    Microsoft::WRL::ComPtr<IUnknown> rootIdentity;
    ASSERT_HRESULT_SUCCEEDED(root.As(&rootIdentity));
    EXPECT_EQ(parent.Get(), rootIdentity.Get());
}

TEST_F(WindowsAccessibilityTest, GivesEachKindItsControlTypeAndPatterns) {
    VARIANT type{};
    ASSERT_HRESULT_SUCCEEDED(getElement(2)->GetPropertyValue(UIA_ControlTypePropertyId, &type));
    EXPECT_EQ(type.lVal, static_cast<LONG>(UIA_ButtonControlTypeId));
    const Microsoft::WRL::ComPtr<IInvokeProvider> play = getPattern<IInvokeProvider>(2, UIA_InvokePatternId);
    ASSERT_TRUE(play);
    EXPECT_HRESULT_SUCCEEDED(play->Invoke());
    EXPECT_FALSE(getPattern<IToggleProvider>(2, UIA_TogglePatternId));

    const Microsoft::WRL::ComPtr<IToggleProvider> music = getPattern<IToggleProvider>(3, UIA_TogglePatternId);
    ASSERT_TRUE(music);
    ToggleState state = ToggleState_Off;
    ASSERT_HRESULT_SUCCEEDED(music->get_ToggleState(&state));
    EXPECT_EQ(state, ToggleState_On);
    EXPECT_HRESULT_SUCCEEDED(music->Toggle());

    // A slider moves one step toward a new value, since the engine knows its number and not its range.
    const Microsoft::WRL::ComPtr<IRangeValueProvider> volume = getPattern<IRangeValueProvider>(4, UIA_RangeValuePatternId);
    ASSERT_TRUE(volume);
    double current = 0.0;
    ASSERT_HRESULT_SUCCEEDED(volume->get_Value(&current));
    EXPECT_DOUBLE_EQ(current, 40.0);
    EXPECT_HRESULT_SUCCEEDED(volume->SetValue(55.0));
    EXPECT_HRESULT_SUCCEEDED(volume->SetValue(10.0));

    // A field tells that it can be edited, since a person types into the native field that it opens.
    const Microsoft::WRL::ComPtr<IValueProvider> name = getPattern<IValueProvider>(5, UIA_ValuePatternId);
    ASSERT_TRUE(name);
    BSTR text = nullptr;
    ASSERT_HRESULT_SUCCEEDED(name->get_Value(&text));
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(std::wstring(text), L"Ana");
    SysFreeString(text);
    BOOL readOnly = TRUE;
    ASSERT_HRESULT_SUCCEEDED(name->get_IsReadOnly(&readOnly));
    EXPECT_EQ(readOnly, FALSE);

    ASSERT_EQ(requests.size(), 4U);
    EXPECT_EQ(requests[0].element, 2U);
    EXPECT_EQ(requests[0].action, AccessibilityElement::Activate);
    EXPECT_EQ(requests[1].element, 3U);
    EXPECT_EQ(requests[2].action, AccessibilityElement::Increment);
    EXPECT_EQ(requests[3].action, AccessibilityElement::Decrement);
}

// Narrator says the number of a range as a share of its minimum and maximum, which the engine does not know, so a slider gives its text through the value pattern as well, the way the other screen readers read it.
TEST_F(WindowsAccessibilityTest, GivesASliderItsValueAsText) {
    const Microsoft::WRL::ComPtr<IValueProvider> volume = getPattern<IValueProvider>(4, UIA_ValuePatternId);
    ASSERT_TRUE(volume);
    BSTR text = nullptr;
    ASSERT_HRESULT_SUCCEEDED(volume->get_Value(&text));
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(std::wstring(text), L"40");
    SysFreeString(text);

    EXPECT_HRESULT_SUCCEEDED(volume->SetValue(L"55"));
    EXPECT_EQ(volume->SetValue(L"loud"), E_INVALIDARG);
    ASSERT_EQ(requests.size(), 1U);
    EXPECT_EQ(requests[0].action, AccessibilityElement::Increment);
}

// The UI checks the option that a radio group holds, which UI Automation reads as the selected item of the group.
TEST_F(WindowsAccessibilityTest, SelectsTheRadioButtonThatIsChecked) {
    const auto bits = [](int value) { return static_cast<std::uint8_t>(value); };
    const std::vector<AccessibilityElement> elements{
        {.id = 1, .role = Role::RadioGroup, .label = "Quality"},
        {.id = 2, .parent = 1, .role = Role::RadioButton, .label = "Low", .actions = bits(AccessibilityElement::Focus | AccessibilityElement::Activate)},
        {.id = 3, .parent = 1, .role = Role::RadioButton, .label = "High", .states = bits(AccessibilityElement::Checked), .actions = bits(AccessibilityElement::Focus | AccessibilityElement::Activate)},
    };
    tree->update(elements, 2, {800.0F, 600.0F});
    const Microsoft::WRL::ComPtr<ISelectionItemProvider> lowOption = getPattern<ISelectionItemProvider>(2, UIA_SelectionItemPatternId);
    const Microsoft::WRL::ComPtr<ISelectionItemProvider> highOption = getPattern<ISelectionItemProvider>(3, UIA_SelectionItemPatternId);
    ASSERT_TRUE(lowOption);
    ASSERT_TRUE(highOption);
    BOOL low = TRUE;
    ASSERT_HRESULT_SUCCEEDED(lowOption->get_IsSelected(&low));
    EXPECT_EQ(low, FALSE);
    BOOL high = FALSE;
    ASSERT_HRESULT_SUCCEEDED(highOption->get_IsSelected(&high));
    EXPECT_EQ(high, TRUE);
}

TEST_F(WindowsAccessibilityTest, TellsTheFocusAndTheElementUnderAPoint) {
    Fragment focused;
    ASSERT_HRESULT_SUCCEEDED(tree->getRoot()->GetFocus(&focused));
    EXPECT_EQ(readName(focused.Get()), L"Play");
    VARIANT focus{};
    ASSERT_HRESULT_SUCCEEDED(getElement(2)->GetPropertyValue(UIA_HasKeyboardFocusPropertyId, &focus));
    EXPECT_EQ(focus.boolVal, VARIANT_TRUE);

    Fragment found;
    ASSERT_HRESULT_SUCCEEDED(tree->getRoot()->ElementProviderFromPoint(20.0, 70.0, &found));
    EXPECT_EQ(readName(found.Get()), L"Music");
    UiaRect bounds{};
    ASSERT_HRESULT_SUCCEEDED(getElement(4)->get_BoundingRectangle(&bounds));
    EXPECT_DOUBLE_EQ(bounds.top, 110.0);
    EXPECT_DOUBLE_EQ(bounds.width, 200.0);
}

// A provider that a client still holds after its element left the tree answers that the element is gone, and acts on nothing.
TEST_F(WindowsAccessibilityTest, AnswersThatAnElementThatLeftIsGone) {
    const Microsoft::WRL::ComPtr<WindowsAccessibilityElement> volume = getElement(4);
    tree->update(makeElements(false), 2, {800.0F, 600.0F});
    VARIANT name{};
    EXPECT_EQ(volume->GetPropertyValue(UIA_NamePropertyId, &name), static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE));
    EXPECT_EQ(volume->SetValue(55.0), static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE));
    EXPECT_TRUE(requests.empty());
    EXPECT_FALSE(navigate(getElement(3).Get(), NavigateDirection_NextSibling));
}

} // namespace haylen::platform
