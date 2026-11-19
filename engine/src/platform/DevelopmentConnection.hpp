#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/net/WebSocket.hpp"

namespace haylen::platform {

class DevelopmentSession;

// The connection of an app in development to the development server of `haylen app run` or `haylen app serve`, which pushes the files saved on the machine of the developer, such as to a browser tab. Every batch arrives as a JSON text message with the paths, followed by one binary message with the bytes of each file. The connection writes them into the overlay of the package that plays, queues their paths in the session and answers with the report of what the batch did. It also runs the lines of the developer console that the server sends and answers with their results, and carries the messages of the Lua debugger and its client, which keep arriving while the debugger halts the app, when everything else waits for the halt to end. Losing the server lets the debugger forget its client. It reconnects on its own, so it outlives restarts of the app and of the server, and it only ever accepts files of the package. It connects at its first pump.
class DevelopmentConnection final {
  public:
    DevelopmentConnection(std::string url, DevelopmentSession& owner, net::WebSocket::BlockedReason blockedReason);
    ~DevelopmentConnection();

    // Delivers what arrived since the last call. It runs on the frame thread.
    void pump();

    // Sends the report of a batch to the server.
    void report(const core::Json& value);

    // Sends a message of the debugger to its client.
    void sendDebug(const core::Json& value);

  private:
    static constexpr float kMaxReconnectDelay = 5.0F;

    void open();

    // Returns the text of a key of a message, or an empty text when the message or the key holds something else.
    [[nodiscard]] static std::string readText(const core::Json& message, const char* key);
    [[nodiscard]] static bool isBatch(const core::Json& message);

    void sendHello();
    void receive(std::string_view data, bool binary);
    void submitLine(const core::Json& message);
    void listCommands();
    void apply();

    DevelopmentSession& session;
    std::string address;
    net::WebSocket::BlockedReason explainBlocked;
    std::unique_ptr<net::WebSocket> socket;

    // The messages that arrived while the debugger halted the app, which the first pump after the halt delivers in order, each with whether it is binary.
    std::vector<std::pair<std::string, bool>> held;
    core::Json batch;
    std::vector<std::vector<std::uint8_t>> contents;
    std::string serverSession;
    std::int64_t revision = 0;
};

} // namespace haylen::platform
