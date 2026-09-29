#pragma once

#import "haylen/platform/apple/HaylenBridge.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace haylen::platform {

// Native side of the platform bridge on Apple platforms: the HaylenBridge handlers, the built-in methods and the JSON that crosses between them and the engine. Handlers may be registered at any time, even before the app starts.
class AppleBridge final {
  public:
    static void setHandler(NSString* method, HaylenHandler handler);
    static void removeHandler(NSString* method);
    static void clearHandlers();

    // Registers device.info, system.locale, system.open_url and haptics.vibrate. A built-in method never replaces a handler the app registered first under the same name.
    static void registerBuiltIns();

    // Runs the handler of a call on the main queue, or fails the call when no handler is registered.
    static void dispatch(std::uint64_t call, std::string_view method, std::string_view paramsJson);

    // Sends a native event to the engine, and drops a payload that JSON cannot hold.
    static void emit(NSString* event, id payload);

  private:
    [[nodiscard]] static NSMutableDictionary<NSString*, HaylenHandler>* getHandlers();
    static void registerBuiltIn(NSString* method, HaylenHandler handler);

    [[nodiscard]] static std::optional<std::string> toJson(id value);
    [[nodiscard]] static id fromJson(std::string_view text);
    [[nodiscard]] static NSString* toString(std::string_view text);
    [[nodiscard]] static NSString* getLanguageTag();

    static void fail(std::uint64_t call, NSString* message);
    static void answer(std::uint64_t call, NSString* method, BOOL ok, id result);
};

} // namespace haylen::platform
