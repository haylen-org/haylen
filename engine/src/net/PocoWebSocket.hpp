#pragma once

#include <Poco/Net/Context.h>
#include <Poco/Net/WebSocket.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "net/WebSocketTransport.hpp"

namespace haylen::net {

// Owns one connection on a thread of its own, which is the only thread that touches the socket, so TLS never sees two threads at once.
class PocoWebSocket final : public WebSocketTransport {
  public:
    PocoWebSocket(std::string url, std::vector<std::string> protocols, Sink target);
    ~PocoWebSocket() override;

    PocoWebSocket(const PocoWebSocket&) = delete;
    PocoWebSocket& operator=(const PocoWebSocket&) = delete;

    void send(std::string data, bool binary) override;
    void ping(std::string payload) override;
    void close(int code, std::string reason) override;

  private:
    struct Outgoing {
        std::string data;
        int flags = 0;
    };

    static constexpr int kConnectSeconds = 10;
    static constexpr auto kCloseTimeout = std::chrono::seconds(5);
    static constexpr int kPollMicroseconds = 2000;
    static constexpr int kNoStatus = 1005;
    static constexpr int kAbnormalClosure = 1006;
    static constexpr int kPingFrame = static_cast<int>(Poco::Net::WebSocket::FRAME_FLAG_FIN) | static_cast<int>(Poco::Net::WebSocket::FRAME_OP_PING);
    static constexpr int kPongFrame = static_cast<int>(Poco::Net::WebSocket::FRAME_FLAG_FIN) | static_cast<int>(Poco::Net::WebSocket::FRAME_OP_PONG);
    static constexpr int kCloseFrame = static_cast<int>(Poco::Net::WebSocket::FRAME_FLAG_FIN) | static_cast<int>(Poco::Net::WebSocket::FRAME_OP_CLOSE);

    // Returns the TLS context every wss:// connection shares, which checks certificates strictly.
    [[nodiscard]] static Poco::Net::Context::Ptr getClientContext();
    [[nodiscard]] static std::unique_ptr<Poco::Net::WebSocket> connect(const std::string& url, const std::vector<std::string>& protocols, std::string& protocol);

    void report(Event event);
    void run(const std::string& url, const std::vector<std::string>& protocols);

    // Non-blocking sends can stop halfway through a frame, and Poco finishes it when the same frame is sent again.
    void write(Poco::Net::WebSocket& socket, std::string_view data, int flags) const;
    void writeClose(Poco::Net::WebSocket& socket, int code, std::string_view reason) const;

    // Alternates between writing queued frames and waiting briefly for incoming ones until either side closes.
    void serve(Poco::Net::WebSocket& socket);

    Sink sink;
    std::mutex mutex;
    std::vector<Outgoing> outgoing;
    std::optional<std::pair<int, std::string>> closeRequest;
    std::atomic<bool> stopping = false;
    std::thread thread;
};

} // namespace haylen::net
