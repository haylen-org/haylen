#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/net/WebSocket.hpp"

namespace haylen::platform {

class DevelopmentSession;

// The connection of an app in development to the development server of `haylen.py`, which pushes the files saved on the machine of the developer, such as to a browser tab. Every batch arrives as a JSON text message with the paths, followed by one binary message with the bytes of each file. The connection writes them into the overlay of the package that plays, queues their paths in the session and answers with the report of what the batch did. It reconnects on its own, so it outlives restarts of the app and of the server, and it only ever accepts files of the package.
class DevelopmentConnection final {
  public:
    DevelopmentConnection(std::string url, DevelopmentSession& owner);

    // Delivers what arrived since the last call. It runs on the frame thread.
    void pump();

    // Sends the report of a batch to the server.
    void report(const core::Json& value);

  private:
    static constexpr float kMaxReconnectDelay = 5.0F;

    void sendHello();
    void receive(std::string_view data, bool binary);
    void apply();

    DevelopmentSession& session;
    std::unique_ptr<net::WebSocket> socket;
    core::Json batch;
    std::vector<std::vector<std::uint8_t>> contents;
    std::string serverSession;
    std::int64_t revision = 0;
};

} // namespace haylen::platform
