#pragma once

#include <functional>
#include <optional>
#include <string>

#include "haylen/platform/Permission.hpp"

namespace haylen::platform {

// The permissions of Apple platforms, read and asked for through the API of each framework: AVFoundation for the camera and the microphone, and Core Location, UserNotifications and Photos, which the runtime loads at run time, so an app links them only when it uses them. A request first checks the usage description of the `Info.plist` that the system needs before it shows a prompt. Every method runs on the main thread, which is the frame thread, and the callbacks run exactly once on any thread.
class ApplePermissions final {
  public:
    static void check(Permission::Kind kind, std::function<void(Permission::Answer answer)> callback);
    static void request(Permission::Kind kind, std::function<void(Permission::Answer answer)> callback);

  private:
    // The key of `NSLocationTemporaryUsageDescriptionDictionary` whose text the request for the precise location shows.
    static constexpr const char* kPrecisePurpose = "preciseLocation";

    // The key of the user defaults that records the request for the location in the background, which the system shows once.
    static constexpr const char* kAskedAlways = "haylen.permissions.backgroundLocation";

    [[nodiscard]] static Permission::Answer checkCapture(Permission::Kind kind);
    static void requestCapture(Permission::Kind kind, std::function<void(Permission::Answer answer)> callback);
    [[nodiscard]] static Permission::Answer checkLocation(Permission::Kind kind);
    static void requestLocation(Permission::Kind kind, std::function<void(Permission::Answer answer)> callback);
    static void checkNotifications(std::function<void(Permission::Answer answer)> callback);
    static void requestNotifications(std::function<void(Permission::Answer answer)> callback);
    [[nodiscard]] static Permission::Answer checkPhotos();
    static void requestPhotos(std::function<void(Permission::Answer answer)> callback);
    [[nodiscard]] static Permission::Answer checkClipboard();

    // The answer of a project that lacks the usage description, after it logged that once, or nothing when the `Info.plist` gives it.
    [[nodiscard]] static std::optional<Permission::Answer> findMissingUsage(const char* key);
    [[nodiscard]] static std::optional<Permission::Answer> findMissingPurpose();

    // Loads a system framework by its name, such as `Photos`, and returns whether its class is there.
    [[nodiscard]] static bool loadFramework(const char* framework, const char* className);

    [[nodiscard]] static Permission::Answer unsupported(std::string message);
};

} // namespace haylen::platform
