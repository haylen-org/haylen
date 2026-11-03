#pragma once

#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/platform/Display.hpp"

namespace haylen::platform {

// The displays that the display manager of Android reports on the UI thread, with the display that shows the activity, which the frame thread reads.
class AndroidDisplays final {
  public:
    // Keeps the displays of the JSON that `HaylenDisplays` writes and tells the running app that they changed.
    static void receive(std::string_view json, int windowDisplay);

    [[nodiscard]] static std::vector<Display> getDisplays();
    [[nodiscard]] static std::string getWindowDisplayId();

  private:
    static std::mutex& mutex;
    static std::vector<Display>& displays;
    static std::string& windowDisplayId;
};

} // namespace haylen::platform
