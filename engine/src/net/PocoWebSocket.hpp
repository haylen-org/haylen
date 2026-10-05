#pragma once

#include <Poco/Crypto/OpenSSLInitializer.h>
#include <Poco/Net/Context.h>
#include <Poco/Net/PollSet.h>
#include <Poco/Net/SecureStreamSocket.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/Net/WebSocket.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "net/WebSocketTransport.hpp"

namespace haylen::net {

// Owns one connection on a thread of its own, which is the only thread that touches the socket, so TLS never sees two threads at once. Closing and destroying never wait for that thread: it keeps what it needs alive by itself, stops as soon as it can and reports nothing more. The thread sleeps until the socket or the app has something for it, and it keeps reading while it waits for room to write, so a connection where both sides send large messages never stalls.
class PocoWebSocket final : public WebSocketTransport {
  public:
    PocoWebSocket(const std::string& url, std::vector<std::string> protocols, std::size_t messageLimit, Sink target);
    ~PocoWebSocket() override;

    PocoWebSocket(const PocoWebSocket&) = delete;
    PocoWebSocket& operator=(const PocoWebSocket&) = delete;

    void send(std::string data, bool binary) override;
    void ping(std::string payload) override;
    void close(int code, std::string reason) override;
    [[nodiscard]] std::size_t getBufferedAmount() const noexcept override;

  private:
    // A frame to write. Messages of the app count in the bytes that wait to be written, and control frames do not.
    struct Outgoing {
        std::string data;
        int flags = 0;
        bool counted = false;
    };

    // What the transport and its thread share. The thread holds it, so it never touches the transport, which the app may destroy at any time, and the mutex keeps the sink quiet once stopping is set.
    struct Connection {
        std::string url;
        std::string host;
        std::uint16_t port = 0;
        std::string path;
        bool secure = false;
        std::vector<std::string> protocols;
        std::size_t maxMessageSize = 0;
        Sink sink;
        std::mutex mutex;
        std::vector<Outgoing> outgoing;
        std::optional<std::pair<int, std::string>> closeRequest;
        bool wakePending = false;
        std::atomic<bool> stopping = false;
        std::atomic<std::size_t> buffered = 0;
        Poco::Net::PollSet poller;
    };

    // Owns what every connection thread shares: OpenSSL, the TLS context of `wss://` and the count of threads that use Poco or OpenSSL. When the process exits it stops every connection and waits for those threads, because the static state of both libraries goes away right after. A thread still resolving its host by then never touches either library again, since resolving has no timeout.
    class Workers final {
      public:
        Workers();
        ~Workers();

        Workers(const Workers&) = delete;
        Workers& operator=(const Workers&) = delete;

        void add(const std::shared_ptr<Connection>& added);

        // Counts the calling thread in, unless the process exits. The caller holds the mutex of a connection the workers have not stopped yet, so the workers still exist.
        [[nodiscard]] bool enter();
        void leave();

        // Returns the TLS context every `wss://` connection shares, which checks certificates strictly.
        [[nodiscard]] Poco::Net::Context::Ptr getClientContext();

      private:
        // Declared first, so OpenSSL stays initialized until everything else of the workers is gone.
        Poco::Crypto::OpenSSLInitializer openSsl;
        std::mutex mutex;
        std::condition_variable idle;
        std::vector<std::weak_ptr<Connection>> connections;
        Poco::Net::Context::Ptr clientContext;
        int active = 0;
        bool exiting = false;
    };

    static constexpr auto kConnectTimeout = std::chrono::seconds(10);
    static constexpr auto kCloseTimeout = std::chrono::seconds(5);

    // Bounds a sleep that every event of the socket or the app interrupts, so an idle connection wakes only once a minute.
    static constexpr auto kIdleWait = std::chrono::minutes(1);
    static constexpr std::size_t kMaxControlPayload = 125;
    static constexpr int kNoStatus = 1005;
    static constexpr int kAbnormalClosure = 1006;
    static constexpr int kPingFrame = static_cast<int>(Poco::Net::WebSocket::FRAME_FLAG_FIN) | static_cast<int>(Poco::Net::WebSocket::FRAME_OP_PING);
    static constexpr int kPongFrame = static_cast<int>(Poco::Net::WebSocket::FRAME_FLAG_FIN) | static_cast<int>(Poco::Net::WebSocket::FRAME_OP_PONG);
    static constexpr int kCloseFrame = static_cast<int>(Poco::Net::WebSocket::FRAME_FLAG_FIN) | static_cast<int>(Poco::Net::WebSocket::FRAME_OP_CLOSE);

    // Returns the workers, created after OpenSSL started so they are destroyed before it cleans up.
    [[nodiscard]] static Workers& getWorkers();

    static void report(Connection& target, Event event);

    // Reports that the connection failed for the reason, a sentence, and that it closed.
    static void fail(Connection& target, const std::string& reason);
    static void wake(Connection& target);

    // Turns an error of the network libraries into the sentence that says what went wrong.
    [[nodiscard]] static std::string describe(const Connection& target, const Poco::Exception& error);

    static void run(const std::shared_ptr<Connection>& target, Workers& workers);
    [[nodiscard]] static std::optional<Poco::Net::SocketAddress> resolve(Connection& target);
    [[nodiscard]] static bool enter(Connection& target, Workers& workers);
    static void connectAndServe(Connection& target, Workers& workers, const Poco::Net::SocketAddress& address);

    // Waits until the socket is ready for one of the modes, the app wakes the thread or the deadline passes, and returns the modes the socket is ready for, or 0.
    static int waitFor(Connection& target, const Poco::Net::Socket& socket, int mode, std::chrono::steady_clock::time_point deadline);

    [[nodiscard]] static Poco::Net::StreamSocket createSocket(const Connection& target, Workers& workers, const Poco::Net::SocketAddress& address);

    // Connects, completes the TLS handshake of `wss://` and upgrades to a WebSocket, or returns nothing once the app abandoned the connection.
    [[nodiscard]] static std::unique_ptr<Poco::Net::WebSocket> open(Connection& target, Workers& workers, const Poco::Net::SocketAddress& address, std::string& protocol);
    [[nodiscard]] static bool connectSocket(Connection& target, Poco::Net::StreamSocket& socket, const Poco::Net::SocketAddress& address, std::chrono::steady_clock::time_point deadline);
    [[nodiscard]] static bool completeHandshake(Connection& target, Poco::Net::SecureStreamSocket& socket, std::chrono::steady_clock::time_point deadline);

    // Writes the frames of the queue until it is empty or the socket has no room, and returns whether it emptied. A non-blocking send can stop halfway through a frame, which stays first, and Poco finishes it when the same frame is sent again.
    static bool flush(Connection& target, Poco::Net::WebSocket& socket, std::deque<Outgoing>& queue);

    // Writes the whole queue, waiting for room, until the deadline, such as the last frames before the connection ends.
    static void drain(Connection& target, Poco::Net::WebSocket& socket, std::deque<Outgoing>& queue, std::chrono::steady_clock::time_point deadline);
    [[nodiscard]] static Outgoing makeClose(int code, std::string_view reason);

    // Ends the connection with status 1009 after the server sent a message larger than the maximum size.
    static void refuseMessage(Connection& target, Poco::Net::WebSocket& socket, std::deque<Outgoing>& queue, bool closing);

    // Writes queued frames and reads incoming ones until either side closes or the app abandons the connection.
    static void serve(Connection& target, Poco::Net::WebSocket& socket);

    std::shared_ptr<Connection> connection;
};

} // namespace haylen::net
