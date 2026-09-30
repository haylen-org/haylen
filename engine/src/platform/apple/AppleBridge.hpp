#pragma once

#import "haylen/platform/apple/HaylenBridge.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/platform/Bridge.hpp"

namespace haylen::platform {

// Native side of the platform bridge on Apple platforms: the HaylenBridge handlers and the JSON that crosses between them and the engine, with NSData for every byte buffer in both directions. Handlers may be registered at any time, even before the app starts.
class AppleBridge final {
  public:
    static void setHandler(NSString* method, HaylenCancellableHandler handler);
    static void removeHandler(NSString* method);
    static void clearHandlers();

    // Runs the handler of a call on the main queue with its parameters, in which NSData values stand for the buffers, or fails the call when no handler is registered.
    static void dispatch(std::uint64_t call, std::string_view method, std::string_view paramsJson, std::span<const std::vector<std::byte>> buffers);

    // Runs the cancel block that the handler of a pending call returned, and drops the answer that may still come.
    static void cancel(std::uint64_t call);

    // Sends a native event to the engine, where NSData values anywhere inside the payload cross as byte buffers, and drops a payload that JSON cannot hold. Events sent while no app runs, such as while the app launches or restarts, wait for the next app.
    static void emit(NSString* event, id payload, const Bridge::EmitOptions& options);

    // Follows whether an app runs. When one starts, the events that waited reach its bridge in order.
    static void setAppRunning(bool value);

    [[nodiscard]] static id fromJson(std::string_view text);

  private:
    // JSON text with the byte buffers it refers to.
    struct Encoded {
        std::string json;
        std::vector<std::vector<std::byte>> buffers;
    };

    struct WaitingEvent {
        std::string event;
        Encoded payload;
        Bridge::EmitOptions options;
    };

    [[nodiscard]] static NSMutableDictionary<NSString*, HaylenCancellableHandler>* getHandlers();

    // The calls that wait for their answer, each with the cancel block its handler returned or NSNull.
    [[nodiscard]] static NSMutableDictionary<NSNumber*, id>* getCalls();

    // Returns the JSON of a value with its NSData values as buffers, or nothing when JSON cannot hold the value, which NSJSONSerialization reports with an exception.
    [[nodiscard]] static std::optional<Encoded> encode(id value);

    // Replaces every NSData with a reference to a buffer, and every reference with its NSData on the way back.
    [[nodiscard]] static id prepare(id value, std::vector<std::vector<std::byte>>& buffers);
    [[nodiscard]] static id restore(id value, NSArray<NSData*>* buffers);

    [[nodiscard]] static NSString* toString(std::string_view text);

    static void fail(std::uint64_t call, NSDictionary* failure);
    static void answer(std::uint64_t call, NSString* method, BOOL ok, id result);

    // The events sent while no app runs, such as the link that opened the app, which native plugins send while it launches.
    static std::vector<WaitingEvent>& waiting;
    static bool running;
};

} // namespace haylen::platform
