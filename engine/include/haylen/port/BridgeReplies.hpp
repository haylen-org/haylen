#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/platform/Bridge.hpp"

namespace haylen::port {

// Carries what the native parts of plugins answer to the engine, from any thread: the results of calls, their events and the ends of screens. Results and events that arrive while no app runs are dropped, and the end of a screen waits for the next app.
class BridgeReplies final {
  public:
    // Answers a call of `BridgeService::dispatch` with a result or a failure as JSON, with the byte buffers that the JSON refers to.
    static void resolve(std::uint64_t id, bool ok, std::string_view resultJson, std::vector<std::vector<std::byte>> buffers);

    // Sends an event of a plugin to the app, which a retained event keeps for the first listener of its name and a batched one delivers in the list of its frame.
    static void emit(std::string_view event, std::string_view payloadJson, std::vector<std::vector<std::byte>> buffers, const platform::Bridge::EmitOptions& options);

    // Ends the screen of `BridgeService::openScreen` with the same id, with a result or a failure, and ends a screen that the platform kept across the end of the process, with the state it saved as JSON text.
    static void finishScreen(std::uint64_t id, bool ok, std::string_view resultJson, std::vector<std::vector<std::byte>> buffers);
    static void restoreScreen(std::string plugin, std::string name, std::string_view stateJson, bool ok, std::string_view resultJson, std::vector<std::vector<std::byte>> buffers);
};

} // namespace haylen::port
