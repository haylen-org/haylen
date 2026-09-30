#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Signal.hpp"
#include "haylen/math/Random.hpp"

namespace haylen::net {

class WebSocketTransport;

// A WebSocket client connection to a `ws://` or `wss://` address. It connects in the background, and its signals fire on the thread that pumps it. After the close event it drops its listeners.
// Native builds run the connection on a thread of its own and check certificates against the Windows root store or, elsewhere, the trust store Varn finds, while the browser build uses the WebSocket of the page. Reconnection works the same way on both.
class WebSocket final {
  public:
    // Reconnecting waits between a lost connection or a failed attempt and the next attempt.
    enum class State : std::uint8_t {
        Connecting,
        Open,
        Closing,
        Closed,
        Reconnecting,
    };

    // Reconnects when the connection drops or an attempt fails, until the app closes the socket. The first attempt waits the initial delay, every further attempt waits the multiplier times longer up to the maximum delay, and jitter shortens each wait by up to that fraction at random, so many clients never retry in step. The socket gives up and closes after the maximum attempts in a row, or never when it is zero.
    struct Reconnect {
        bool enabled = false;
        float initialDelay = 0.5F;
        float maxDelay = 30.0F;
        float multiplier = 2.0F;
        float jitter = 0.5F;
        int maxAttempts = 0;
    };

    // Returns the current time in seconds, which tests replace to drive the delays. Without one, the socket reads the steady clock.
    using Clock = std::function<double()>;

    // Native builds refuse a message of the server larger than the maximum size in bytes, at most 2147483647, and close the connection with status 1009. The failure hint is a sentence that the message of every failure ends with, such as what the project of the app lacks for network access.
    struct Options {
        std::vector<std::string> protocols;
        std::size_t maxMessageSize = 16U * 1024U * 1024U;
        Reconnect reconnect;
        std::string failureHint;
        Clock clock;
        std::uint64_t seed = 0;
    };

    explicit WebSocket(std::string address, Options options = kDefaultOptions);
    ~WebSocket();

    WebSocket(const WebSocket&) = delete;
    WebSocket& operator=(const WebSocket&) = delete;

    // Returns `connecting`, `open`, `closing`, `closed` or `reconnecting`.
    [[nodiscard]] static std::string_view stateName(State value) noexcept;

    // Returns the wait before the attempt, which counts from one, before jitter shortens it.
    [[nodiscard]] static float getBackoff(const Reconnect& settings, int number) noexcept;

    void send(std::string_view text);
    void sendBinary(std::span<const std::uint8_t> bytes);

    // Sends a ping frame with a payload of at most 125 bytes, which the server answers with a pong that `ponged` reports. Browsers cannot send ping frames, so the browser build throws `std::logic_error`.
    void ping(std::string_view payload = {});

    // Starts the closing handshake, or ends a wait for the next attempt. The code is 1000 or between 3000 and 4999 and the reason fits in 123 bytes, as browsers require.
    void close(int code = 1000, std::string_view reason = {});

    // Delivers the events that arrived since the last call and starts the next attempt once its wait is over.
    void pump();
    void dropListeners() noexcept;

    [[nodiscard]] State getState() const noexcept {
        return state;
    }
    [[nodiscard]] const std::string& getUrl() const noexcept {
        return url;
    }
    [[nodiscard]] const std::string& getProtocol() const noexcept {
        return protocol;
    }

    // Counts the attempts since the connection was last open, which is zero while it is open.
    [[nodiscard]] int getAttempt() const noexcept {
        return attempt;
    }

    core::Signal<> opened;
    core::Signal<std::string_view, bool> received;
    core::Signal<std::string_view> failed;

    // Fires with the payload of every pong frame the server sends, which answers a ping or keeps the connection alive on its own.
    core::Signal<std::string_view> ponged;

    // Fires when an open connection ends, before the socket either reconnects or closes, and its listeners already see the reconnecting or closed state.
    core::Signal<int, std::string_view> disconnected;

    // Fires with the attempt number and the wait in seconds every time the socket schedules an attempt.
    core::Signal<int, float> reconnecting;

    // Fires once when the socket is done: the app closed it, reconnection is off or it gave up.
    core::Signal<int, std::string_view> closed;

  private:
    struct Inbox;

    static const Options& kDefaultOptions;
    static constexpr std::size_t kMaxPingPayload = 125;

    [[nodiscard]] static double getSteadySeconds();

    void connect();
    [[nodiscard]] bool canReconnect() const noexcept;
    void scheduleReconnect();
    [[nodiscard]] std::string describeFailure(const std::string& message) const;

    std::string url;
    std::vector<std::string> protocols;
    std::size_t maxMessageSize = 0;
    Reconnect reconnect;
    std::string failureHint;
    Clock clock;
    math::Random random;
    std::string protocol;
    State state = State::Connecting;
    std::shared_ptr<Inbox> inbox;
    std::unique_ptr<WebSocketTransport> transport;
    double nextAttemptAt = 0.0;
    int attempt = 0;
    bool connected = false;
    bool closeRequested = false;
};

} // namespace haylen::net
