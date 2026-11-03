#pragma once

#include <string>
#include <vector>

#include "haylen/platform/Display.hpp"

namespace haylen::platform {

// The screens that the browser shows the page, which `platform/web/haylen-runtime.js` describes: the screen of the page, and every screen of the device once the page holds the permission `screens`, which the Window Management API asks the person for. Browsers tell no refresh rates and no modes.
class WebDisplays final {
  public:
    [[nodiscard]] static std::vector<Display> getDisplays();
    [[nodiscard]] static std::string getWindowDisplayId();
};

} // namespace haylen::platform
