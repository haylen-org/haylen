#include "haylen/net/WebSocket.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
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

const WebSocket::Options WebSocket::kDefaultOptions{};

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

float WebSocket::getBackoff(const Reconnect& reconnect, int attempt) noexcept {
    const double grown = static_cast<double>(reconnect.initialDelay) * std::pow(static_cast<double>(reconnect.multiplier), std::max(attempt - 1, 0));
    return static_cast<float>(std::min(grown, static_cast<double>(reconnect.maxDelay)));
}

double WebSocket::getSteadySeconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

WebSocket::WebSocket(std::string address, Options options) : url(std::move(address)), protocols(std::move(options.protocols)), reconnect(options.reconnect), clock(options.clock ? std::move(options.clock) : Clock(&getSteadySeconds)), random(options.seed != 0 ? options.seed : std::random_device{}()), inbox(std::make_shared<Inbox>()) {
    if (!url.starts_with("ws://") && !url.starts_with("wss://")) {
        throw std::invalid_argument("A WebSocket address starts with ws:// or wss://: " + url);
    }
    if (reconnect.initialDelay < 0.0F || reconnect.maxDelay < reconnect.initialDelay || reconnect.multiplier < 1.0F || reconnect.jitter < 0.0F || reconnect.jitter > 1.0F || reconnect.maxAttempts < 0) {
        throw std::invalid_argument("WebSocket reconnection needs delays from zero up with the maximum at least the initial one, a multiplier of at least 1, a jitter between 0 and 1 and a maximum of attempts of at least 0.");
    }
    connect();
}

WebSocket::~WebSocket() = default;

void WebSocket::connect() {
    state = State::Connecting;
    // clang-format off
    transport = WebSocketTransport::open(url, protocols, [delivery = inbox](WebSocketTransport::Event event) {
        const std::scoped_lock lock(delivery->mutex);
        delivery->events.push_back(std::move(event));
    });
    // clang-format on
}

void WebSocket::send(std::string_view text) {
    if (state != State::Open) {
        throw std::logic_error("The WebSocket to " + url + " is not open.");
    }
    transport->send(std::string(text), false);
}

void WebSocket::sendBinary(std::span<const std::uint8_t> bytes) {
    if (state != State::Open) {
        throw std::logic_error("The WebSocket to " + url + " is not open.");
    }
    transport->send(std::string(bytes.begin(), bytes.end()), true);
}

void WebSocket::ping(std::string_view payload) {
    if (payload.size() > kMaxPingPayload) {
        throw std::invalid_argument("A WebSocket ping carries at most 125 bytes.");
    }
    if (state != State::Open) {
        throw std::logic_error("The WebSocket to " + url + " is not open.");
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

    // No connection exists while the socket waits for its next attempt, so it closes on the next pump.
    if (state == State::Reconnecting) {
        state = State::Closing;
        const std::scoped_lock lock(inbox->mutex);
        inbox->events.push_back({.kind = WebSocketTransport::Event::Kind::Closed, .text = std::string(reason), .code = code});
        return;
    }
    state = State::Closing;
    transport->close(code, std::string(reason));
}

bool WebSocket::scheduleReconnect() {
    if (!reconnect.enabled || closeRequested || (reconnect.maxAttempts > 0 && attempt >= reconnect.maxAttempts)) {
        return false;
    }
    ++attempt;
    const float delay = getBackoff(reconnect, attempt) * (1.0F - reconnect.jitter * random.nextFloat());
    state = State::Reconnecting;
    nextAttemptAt = clock() + static_cast<double>(delay);
    reconnecting.emit(attempt, delay);
    return true;
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
            failed.emit(event.text);
            break;
        case WebSocketTransport::Event::Kind::Closed:
            // A transport reports nothing after closing, so the next attempt opens a fresh one.
            transport.reset();
            if (std::exchange(connected, false)) {
                disconnected.emit(event.code, event.text);
            }
            if (scheduleReconnect()) {
                break;
            }
            state = State::Closed;
            closed.emit(event.code, event.text);
            dropListeners();
            return;
        }
    }
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
