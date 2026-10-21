#pragma once

namespace haylen::platform {

// What the player tells Apple platforms: whether an app runs, which native events wait for.
class AppleRuntime final {
  public:
    static void setAppRunning(bool value);
};

} // namespace haylen::platform
