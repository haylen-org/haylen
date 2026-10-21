#pragma once

namespace haylen::sokol {

// The classes that `sokol_app` creates on Apple platforms in place of its own: the application delegate, which hands the events of the app to native plugins, and the view controller of the window, which answers UIKit about the status bar.
class AppleClasses final {
  public:
    [[nodiscard]] static const char* getDelegateClass() noexcept;
    [[nodiscard]] static const char* getViewControllerClass() noexcept;
};

} // namespace haylen::sokol
