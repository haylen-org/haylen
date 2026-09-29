#pragma once

#include <cstdint>
#include <mutex>
#include <string_view>

namespace haylen::platform {

class Bridge;

// Carries the replies and events of native code to the bridge of the running engine. Native code may answer from any thread, so the lock keeps the bridge alive until a reply has been queued, and detaching waits for replies in flight.
class BridgeRelay final {
  public:
    // Connects native replies and events to the bridge of the running engine, or disconnects them with null.
    static void attach(Bridge* value) noexcept;

    // Thread-safe entry points for native handlers.
    static void resolve(std::uint64_t id, bool ok, std::string_view resultJson);
    static void emit(std::string_view event, std::string_view payloadJson);

  private:
    static std::mutex mutex;
    static Bridge* bridge;
};

} // namespace haylen::platform
