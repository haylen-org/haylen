#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace haylen::net {

// The platform side of a WebSocket. Destroying it ends the connection without reporting anything more.
class WebSocketTransport {
  public:
    struct Event {
        enum class Kind : std::uint8_t {
            Opened,
            Received,
            Ponged,
            Closed,
            Failed,
        };

        Kind kind = Kind::Opened;
        std::string text;
        bool binary = false;
        int code = 0;
    };

    // Receives transport events from any thread. A transport reports a failure before its closed event, and nothing after it.
    using Sink = std::function<void(Event)>;

    virtual ~WebSocketTransport() = default;

    virtual void send(std::string data, bool binary) = 0;
    virtual void ping(std::string payload) = 0;
    virtual void close(int code, std::string reason) = 0;

    // The bytes of messages that were sent and still wait to be written to the network.
    [[nodiscard]] virtual std::size_t getBufferedAmount() const noexcept = 0;

    // Opens the transport of the platform: Poco on native builds, which refuses messages larger than the maximum size, and the WebSocket of the page in the browser, which applies the limits of the browser.
    [[nodiscard]] static std::unique_ptr<WebSocketTransport> open(const std::string& url, const std::vector<std::string>& protocols, std::size_t maxMessageSize, Sink sink);
};

} // namespace haylen::net
