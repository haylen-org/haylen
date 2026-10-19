#pragma once

namespace haylen::platform {

// What the Sokol runtime tells Apple platforms and asks of them: the application delegate that sokol_app creates, which hands the events of the app to native plugins, the view controller of its window, and whether an app runs, which native events wait for.
class AppleRuntime final {
  public:
    [[nodiscard]] static const char* getDelegateClass() noexcept;
    [[nodiscard]] static const char* getViewControllerClass() noexcept;
    static void setAppRunning(bool value);
};

} // namespace haylen::platform
