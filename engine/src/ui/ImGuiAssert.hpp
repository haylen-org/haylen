#pragma once

namespace haylen::ui {

// Dear ImGui reports misuse, such as an `End` without a `Begin`, through `IM_ASSERT`. Throwing turns it into a Lua error or the engine error screen instead of aborting the app, with the reason that Dear ImGui gives its check and without the code of the check.
class ImGuiAssert final {
  public:
    [[noreturn]] static void fail(const char* expression);
};

} // namespace haylen::ui
