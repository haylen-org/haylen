#pragma once

#include <functional>

#include "haylen/platform/Permission.hpp"

namespace haylen::platform {

// The permissions of the engine on Apple platforms: every screen, which an iPhone or an iPad of iOS 27 and later gets through the scene accessory of external displays, and the local network, which the system asks for at the first use. Every method runs on the main thread, which is the frame thread, and the callbacks run exactly once on any thread.
class ApplePermissions final {
  public:
    static void check(Permission::Kind kind, std::function<void(Permission::Answer answer)> callback);
    static void request(Permission::Kind kind, std::function<void(Permission::Answer answer)> callback);
};

} // namespace haylen::platform
