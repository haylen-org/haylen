#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/net/WebSocket.hpp"

namespace haylen::platform {

class DevelopmentSession;

// The connection of an app in development to the development server of `haylen.py`, which pushes the files saved on the machine of the developer, such as to a browser tab. Every batch arrives as a JSON text message with the paths, followed by one binary message with the bytes of each file. The connection writes them into the overlay of the package that plays, queues their paths in the session and answers with the report of what the batch did. It also runs the lines of the developer console that the server sends and answers with their results. It reconnects on its own, so it outlives restarts of the app and of the server, and it only ever accepts files of the package. It connects at its first pump, unless something holds it, such as the permission for the local network that a server outside the device needs on Android.
class DevelopmentConnection final {
  public:
    DevelopmentConnection(std::string url, DevelopmentSession& owner, net::WebSocket::BlockedReason blockedReason);

    // Keeps the connection from connecting, and returns what lets it connect, with no refusal, or keeps it away for good with the sentence that says why, from any thread.
    [[nodiscard]] std::function<void(std::optional<std::string> refusal)> hold();

    // Whether the address of the server is on this device, such as the one that "adb reverse" forwards, which needs no access to the local network.
    [[nodiscard]] bool isOnThisDevice() const;

    // Delivers what arrived since the last call. It runs on the frame thread.
    void pump();

    // Sends the report of a batch to the server.
    void report(const core::Json& value);

  private:
    static constexpr float kMaxReconnectDelay = 5.0F;

    // What decides whether a held connection connects, shared with whatever answers from another thread.
    struct Gate {
        std::mutex mutex;
        bool answered = false;
        std::optional<std::string> refusal;
    };

    // Opens the socket once nothing holds the connection.
    void open();
    [[nodiscard]] bool isHeld();

    // Returns the text of a key of a message, or an empty text when the message or the key holds something else.
    [[nodiscard]] static std::string readText(const core::Json& message, const char* key);
    [[nodiscard]] static bool isBatch(const core::Json& message);

    void sendHello();
    void receive(std::string_view data, bool binary);
    void submitLine(const core::Json& message);
    void apply();

    DevelopmentSession& session;
    std::string address;
    net::WebSocket::BlockedReason explainBlocked;
    std::shared_ptr<Gate> gate;
    bool refused = false;
    std::unique_ptr<net::WebSocket> socket;
    core::Json batch;
    std::vector<std::vector<std::uint8_t>> contents;
    std::string serverSession;
    std::int64_t revision = 0;
};

} // namespace haylen::platform
