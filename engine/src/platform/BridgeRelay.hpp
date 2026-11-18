#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string_view>
#include <vector>

#include "haylen/platform/Bridge.hpp"
#include "platform/KeptEvents.hpp"

namespace haylen::platform {

// Carries the replies and events of native code to the bridge of the running engine, which the platform plugin attaches when the app starts. Native code may answer from any thread, so the lock keeps the bridge alive until a reply has been queued, and detaching waits for replies in flight.
class BridgeRelay final {
  public:
    // Connects the bridge of the app that starts, which receives the events that the relay kept while no app ran, in the order they came.
    static void attach(Bridge& value);

    // Disconnects the relay when it still leads to this bridge, so an engine that stops late never disconnects the one that replaced it.
    static void detach(const Bridge& value) noexcept;

    // Thread-safe entry points for native handlers, with the byte buffers that the JSON refers to. A retained event waits in the bridge for the first listener of its name, a batched one reaches the listeners in the list of its frame, and events that arrive with no app running are dropped.
    static void resolve(std::uint64_t id, bool ok, std::string_view resultJson, std::vector<std::vector<std::byte>> buffers = {});
    static void emit(std::string_view event, std::string_view payloadJson, std::vector<std::vector<std::byte>> buffers, const Bridge::EmitOptions& options);

    // Sends the event like `emit`, or keeps it while no app runs for the bridge of the next app, up to `KeptEvents::kLimitPerName` of each name, so the events that native libraries send while the app launches or restarts reach it. Throws `std::invalid_argument` for a kept event whose JSON names a data view that is gone.
    static void emitOrKeep(KeptEvents::Event event);

  private:
    static std::mutex& mutex;
    static Bridge* bridge;
    static KeptEvents& kept;
};

} // namespace haylen::platform
