#pragma once

#import "haylen/platform/apple/HaylenBridge.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace haylen::platform {

// Native side of the platform bridge on Apple platforms: the HaylenBridge handlers and the JSON that crosses between them and the engine. Handlers may be registered at any time, even before the app starts.
class AppleBridge final {
  public:
    static void setHandler(NSString* method, HaylenCancellableHandler handler);
    static void removeHandler(NSString* method);
    static void clearHandlers();

    // Runs the handler of a call on the main queue, or fails the call when no handler is registered.
    static void dispatch(std::uint64_t call, std::string_view method, std::string_view paramsJson);

    // Runs the cancel block that the handler of a pending call returned, and drops the answer that may still come.
    static void cancel(std::uint64_t call);

    // Sends a native event to the engine, which keeps a retained one for the first listener of its name, and drops a payload that JSON cannot hold. Events sent while no app runs, such as while the app launches or restarts, wait for the next app.
    static void emit(NSString* event, id payload, bool retain);

    // Follows whether an app runs. When one starts, the events that waited reach its bridge in order.
    static void setAppRunning(bool value);

    [[nodiscard]] static id fromJson(std::string_view text);

  private:
    struct WaitingEvent {
        std::string event;
        std::string payload;
        bool retain = false;
    };

    [[nodiscard]] static NSMutableDictionary<NSString*, HaylenCancellableHandler>* getHandlers();

    // The calls that wait for their answer, each with the cancel block its handler returned or NSNull.
    [[nodiscard]] static NSMutableDictionary<NSNumber*, id>* getCalls();

    [[nodiscard]] static std::optional<std::string> toJson(id value);
    [[nodiscard]] static NSString* toString(std::string_view text);

    static void fail(std::uint64_t call, NSDictionary* failure);
    static void answer(std::uint64_t call, NSString* method, BOOL ok, id result);

    // The events sent while no app runs, such as the link that opened the app, which native plugins send while it launches.
    static std::vector<WaitingEvent>& waiting;
    static bool running;
};

} // namespace haylen::platform
