#pragma once

namespace haylen::ui {

// Dear ImGui reports misuse, such as an `End` without a `Begin`, through `IM_ASSERT`. Throwing turns it into a Lua error or the engine error screen instead of aborting the app.
class ImGuiAssert final {
  public:
    [[noreturn]] static void fail(const char* expression, const char* file, int line);
};

} // namespace haylen::ui
