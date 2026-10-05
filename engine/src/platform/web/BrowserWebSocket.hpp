#pragma once

#include <map>
#include <string>
#include <vector>

#include "net/WebSocketTransport.hpp"

namespace haylen::net {

// A WebSocket of the page, which `platform/web/haylen-runtime.js` keeps by the id this transport gives it.
class BrowserWebSocket final : public WebSocketTransport {
  public:
    BrowserWebSocket(const std::string& url, const std::vector<std::string>& protocols, Sink sink);
    ~BrowserWebSocket() override;

    BrowserWebSocket(const BrowserWebSocket&) = delete;
    BrowserWebSocket& operator=(const BrowserWebSocket&) = delete;

    void send(std::string data, bool binary) override;

    // Browsers answer the pings of a server on their own, but JavaScript has no way to send one.
    void ping(std::string payload) override;
    void close(int code, std::string reason) override;
    [[nodiscard]] std::size_t getBufferedAmount() const noexcept override;

    // Hands an event of the page to the transport with the id, which ignores it once the transport is gone.
    static void deliver(int socket, Event event);

  private:
    [[nodiscard]] static std::map<int, Sink>& getSinks();

    static inline int nextId = 1;

    int id;
};

} // namespace haylen::net
