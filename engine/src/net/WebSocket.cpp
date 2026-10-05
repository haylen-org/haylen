#include "haylen/net/WebSocket.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <mutex>
#include <random>
#include <stdexcept>
#include <utility>

#include "net/WebSocketTransport.hpp"

namespace haylen::net {

struct WebSocket::Inbox {
    std::mutex mutex;
    std::vector<WebSocketTransport::Event> events;
};

const WebSocket::Options& WebSocket::kDefaultOptions = *new const Options();

std::string_view WebSocket::stateName(State value) noexcept {
    switch (value) {
    case State::Connecting:
        return "connecting";
    case State::Open:
        return "open";
    case State::Closing:
        return "closing";
    case State::Closed:
        return "closed";
    case State::Reconnecting:
        return "reconnecting";
    }
    return "closed";
}

float WebSocket::getBackoff(const Reconnect& settings, int number) noexcept {
    const double grown = static_cast<double>(settings.initialDelay) * std::pow(static_cast<double>(settings.multiplier), std::max(number - 1, 0));
    return static_cast<float>(std::min(grown, static_cast<double>(settings.maxDelay)));
}

double WebSocket::getSteadySeconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

WebSocket::WebSocket(std::string address, Options options) : url(std::move(address)), protocols(std::move(options.protocols)), maxMessageSize(options.maxMessageSize), connectTimeout(options.connectTimeout), reconnect(options.reconnect), failureHint(std::move(options.failureHint)), clock(options.clock ? std::move(options.clock) : Clock(&getSteadySeconds)), random(options.seed != 0 ? options.seed : std::random_device{}()), inbox(std::make_shared<Inbox>()) {
    if (!url.starts_with("ws://") && !url.starts_with("wss://")) {
        throw std::invalid_argument("The WebSocket address \"" + url + "\" must start with \"ws://\" or \"wss://\".");
    }
    if (maxMessageSize == 0 || !std::in_range<int>(maxMessageSize)) {
        throw std::invalid_argument("A WebSocket needs a maximum message size between 1 and 2147483647 bytes.");
    }
    if (!(connectTimeout > 0.0F)) {
        throw std::invalid_argument("A WebSocket needs a connect timeout above 0 seconds.");
    }
    if (reconnect.initialDelay < 0.0F || reconnect.maxDelay < reconnect.initialDelay || reconnect.multiplier < 1.0F || reconnect.jitter < 0.0F || reconnect.jitter > 1.0F || reconnect.maxAttempts < 0) {
        throw std::invalid_argument("WebSocket reconnection needs delays from zero up with the maximum at least the initial one, a multiplier of at least 1, a jitter between 0 and 1 and a maximum of attempts of at least 0.");
    }
    connect();
}

WebSocket::~WebSocket() = default;

void WebSocket::connect() {
    state = State::Connecting;
    attemptStartedAt = clock();
    // clang-format off
    transport = WebSocketTransport::open(url, protocols, maxMessageSize, [delivery = inbox](WebSocketTransport::Event event) {
        const std::scoped_lock lock(delivery->mutex);
        delivery->events.push_back(std::move(event));
    });
    // clang-format on
}

void WebSocket::send(std::string_view text) {
    if (state != State::Open) {
        throw std::logic_error("The WebSocket to \"" + url + "\" is not open.");
    }
    transport->send(std::string(text), false);
}

void WebSocket::sendBinary(std::span<const std::uint8_t> bytes) {
    if (state != State::Open) {
        throw std::logic_error("The WebSocket to \"" + url + "\" is not open.");
    }
    transport->send(std::string(bytes.begin(), bytes.end()), true);
}

void WebSocket::ping(std::string_view payload) {
    if (payload.size() > kMaxPingPayload) {
        throw std::invalid_argument("A WebSocket ping carries at most 125 bytes.");
    }
    if (state != State::Open) {
        throw std::logic_error("The WebSocket to \"" + url + "\" is not open.");
    }
    transport->ping(std::string(payload));
}

void WebSocket::close(int code, std::string_view reason) {
    if (code != 1000 && (code < 3000 || code > 4999)) {
        throw std::invalid_argument("A WebSocket closes with code 1000 or a code between 3000 and 4999.");
    }
    if (reason.size() > 123) {
        throw std::invalid_argument("A WebSocket close reason fits in 123 bytes.");
    }
    if (state == State::Closing || state == State::Closed) {
        return;
    }
    closeRequested = true;
    state = State::Closing;

    // Without a connection, such as while the socket waits for its next attempt, it closes on the next pump.
    if (!transport) {
        const std::scoped_lock lock(inbox->mutex);
        inbox->events.push_back({.kind = WebSocketTransport::Event::Kind::Closed, .text = std::string(reason), .code = code});
        return;
    }
    transport->close(code, std::string(reason));
}

std::size_t WebSocket::getBufferedAmount() const noexcept {
    return transport ? transport->getBufferedAmount() : 0;
}

bool WebSocket::canReconnect() const noexcept {
    return reconnect.enabled && !closeRequested && (reconnect.maxAttempts == 0 || attempt < reconnect.maxAttempts);
}

void WebSocket::scheduleReconnect() {
    ++attempt;
    const float delay = getBackoff(reconnect, attempt) * (1.0F - reconnect.jitter * random.nextFloat());
    nextAttemptAt = clock() + static_cast<double>(delay);
    reconnecting.emit(attempt, delay);
}

void WebSocket::pump() {
    if (state == State::Reconnecting && clock() >= nextAttemptAt) {
        connect();
    }

    std::vector<WebSocketTransport::Event> events;
    {
        const std::scoped_lock lock(inbox->mutex);
        events.swap(inbox->events);
    }

    for (const WebSocketTransport::Event& event : events) {
        switch (event.kind) {
        case WebSocketTransport::Event::Kind::Opened:
            protocol = event.text;
            attempt = 0;
            connected = true;
            state = state == State::Closing ? State::Closing : State::Open;
            opened.emit();
            break;
        case WebSocketTransport::Event::Kind::Received:
            received.emit(event.text, event.binary);
            break;
        case WebSocketTransport::Event::Kind::Ponged:
            ponged.emit(event.text);
            break;
        case WebSocketTransport::Event::Kind::Failed:
            failed.emit(describeFailure(event.text));
            break;
        case WebSocketTransport::Event::Kind::Closed:
            if (finishConnection(event.code, event.text)) {
                return;
            }
            break;
        }
    }
    checkConnectTimeout();
}

// A transport reports nothing after closing, so the next attempt opens a fresh one, and the disconnect listeners already see the state that follows.
bool WebSocket::finishConnection(int code, std::string_view reason) {
    transport.reset();
    state = canReconnect() ? State::Reconnecting : State::Closed;
    if (std::exchange(connected, false)) {
        disconnected.emit(code, reason);
    }

    // A disconnect listener that closed the socket ends it on the next pump, with its own code.
    if (state == State::Closing) {
        return false;
    }
    if (state == State::Reconnecting) {
        scheduleReconnect();
        return false;
    }
    closed.emit(code, reason);
    dropListeners();
    return true;
}

// An attempt that does not open in time ends like a failed one, which on the web also covers browsers that wait minutes for a server that never answers.
void WebSocket::checkConnectTimeout() {
    if (state != State::Connecting || !transport || clock() - attemptStartedAt < static_cast<double>(connectTimeout)) {
        return;
    }
    // The abandoned transport may still deliver into the inbox it was given, which a fresh inbox leaves behind.
    inbox = std::make_shared<Inbox>();
    transport.reset();
    failed.emit(describeFailure(std::format("The WebSocket connection to \"{}\" did not open within {:g} seconds.", url, connectTimeout)));
    (void)finishConnection(kAbnormalClosure, {});
}

// The messages of the transport, such as the ones of Poco, may end without a period, and the hint follows as a sentence of its own.
std::string WebSocket::describeFailure(const std::string& message) const {
    if (failureHint.empty()) {
        return message;
    }
    return message + (message.ends_with('.') ? " " : ". ") + failureHint;
}

void WebSocket::dropListeners() noexcept {
    opened.clear();
    received.clear();
    failed.clear();
    ponged.clear();
    disconnected.clear();
    reconnecting.clear();
    closed.clear();
}

} // namespace haylen::net
