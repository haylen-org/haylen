#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "haylen/platform/Display.hpp"
#include "haylen/platform/Window.hpp"

namespace haylen::platform {

// The screens that the browser shows the page, which `platform/web/haylen-runtime.js` describes: the screen of the page, and every screen of the device once the person lets the page see them through the Window Management API. Browsers tell no refresh rates and no modes.
class WebDisplays final {
  public:
    [[nodiscard]] static std::vector<Display> getDisplays();
    [[nodiscard]] static std::string getWindowDisplayId();

    // Asks the browser to show every screen, which the page answers through `answer` with the same id.
    static void request(std::function<void(Window::DisplayAccess access)> callback);
    static void answer(double id, Window::DisplayAccess access);

  private:
    static std::unordered_map<std::uint64_t, std::function<void(Window::DisplayAccess)>>& requests;
    static std::uint64_t nextRequest;
};

} // namespace haylen::platform
