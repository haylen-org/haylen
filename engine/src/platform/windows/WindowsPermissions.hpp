#pragma once

#include <functional>

#include "haylen/platform/Permission.hpp"

namespace haylen::platform {

// The permissions of Windows: the camera, the microphone and the location, which the privacy settings of the system grant apps and which the engine reads and asks for through the capabilities of `AppCapability`, and the other kinds, which Windows tells apps nothing of or never gates. Every method runs on the frame thread, which is the thread of the window, and the answer of a request comes on any thread.
class WindowsPermissions final {
  public:
    static void check(Permission::Kind kind, std::function<void(Permission::Answer answer)> callback);
    static void request(Permission::Kind kind, std::function<void(Permission::Answer answer)> callback);

  private:
    // The name of the capability of the kind, or nothing for a kind that Windows does not gate through a capability.
    [[nodiscard]] static const wchar_t* getCapabilityName(Permission::Kind kind) noexcept;

    // The answer of a kind without a capability: notifications, which the person may turn off in the settings without telling the app, are unknown, and the others are granted.
    [[nodiscard]] static Permission::Answer answerWithoutCapability(Permission::Kind kind);

    [[nodiscard]] static Permission::Answer answerStatus(int status, const wchar_t* capability);

    // Starts the Windows Runtime on the frame thread once, and answers whether it runs.
    [[nodiscard]] static bool startRuntime();

    static bool runtimeStarted;
};

} // namespace haylen::platform
