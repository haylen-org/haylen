#include "platform/windows/WindowsAccessibility.hpp"

#include <ole2.h>
#include <uiautomation.h>

#include "platform/AccessibilityRelay.hpp"
#include "platform/windows/WindowsAccessibilityRoot.hpp"
#include "platform/windows/WindowsAccessibilityTree.hpp"
#include "sokol_app.h"

namespace haylen::platform {

std::shared_ptr<WindowsAccessibilityTree> WindowsAccessibility::tree;
std::atomic<bool> WindowsAccessibility::asked = false;

std::optional<LRESULT> WindowsAccessibility::answer(HWND window, WPARAM wParam, LPARAM lParam) {
    if (static_cast<long>(lParam) != static_cast<long>(UiaRootObjectId)) {
        return std::nullopt;
    }
    asked = true;
    return UiaReturnRawElementProvider(window, wParam, lParam, getTree(window).getRoot().Get());
}

bool WindowsAccessibility::isWanted() noexcept {
    BOOL screenReader = FALSE;
    SystemParametersInfoW(SPI_GETSCREENREADER, 0, &screenReader, 0);
    return asked || screenReader != FALSE;
}

void WindowsAccessibility::update(std::span<const AccessibilityElement> elements, std::uint64_t focused) {
    getTree(static_cast<HWND>(const_cast<void*>(sapp_win32_get_hwnd()))).update(elements, focused, {sapp_widthf(), sapp_heightf()});
}

void WindowsAccessibility::announce(std::string_view text, bool interrupt) {
    getTree(static_cast<HWND>(const_cast<void*>(sapp_win32_get_hwnd()))).announce(text, interrupt);
}

void WindowsAccessibility::release() noexcept {
    if (tree == nullptr) {
        return;
    }
    tree->disconnect();
    UiaReturnRawElementProvider(tree->getWindow(), 0, 0, nullptr);
    tree.reset();
}

// The window keeps one tree for every app that runs in it, which each app fills with its own elements.
WindowsAccessibilityTree& WindowsAccessibility::getTree(HWND window) {
    if (tree == nullptr) {
        tree = std::make_shared<WindowsAccessibilityTree>(window, [](const Accessibility::Request& request) { AccessibilityRelay::perform(request); });
    }
    return *tree;
}

} // namespace haylen::platform
