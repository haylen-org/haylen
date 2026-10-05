#include "platform/web/BrowserWebSocket.hpp"

#include <emscripten/emscripten.h>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <utility>

#include "haylen/core/Json.hpp"

// clang-format off
EM_JS(void, haylen_js_socket_open, (int id, const char* url, const char* protocols), {
    Module.haylen.openSocket(id, UTF8ToString(url), JSON.parse(UTF8ToString(protocols)));
});

EM_JS(void, haylen_js_socket_send, (int id, const char* data, int size, int binary), {
    Module.haylen.sendSocket(id, HEAPU8.slice(data, data + size), binary !== 0);
});

EM_JS(void, haylen_js_socket_close, (int id, int code, const char* reason), {
    Module.haylen.closeSocket(id, code, UTF8ToString(reason));
});

EM_JS(double, haylen_js_socket_buffered, (int id), {
    return Module.haylen.socketBufferedAmount(id);
});

EM_JS(void, haylen_js_socket_release, (int id), {
    Module.haylen.releaseSocket(id);
});
// clang-format on

namespace haylen::net {

std::unique_ptr<WebSocketTransport> WebSocketTransport::open(const std::string& url, const std::vector<std::string>& protocols, std::size_t, Sink sink) {
    return std::make_unique<BrowserWebSocket>(url, protocols, std::move(sink));
}

std::map<int, WebSocketTransport::Sink>& BrowserWebSocket::getSinks() {
    static std::map<int, Sink>& registered = *new std::map<int, Sink>();
    return registered;
}

BrowserWebSocket::BrowserWebSocket(const std::string& url, const std::vector<std::string>& protocols, Sink sink) : id(nextId++) {
    getSinks().emplace(id, std::move(sink));
    haylen_js_socket_open(id, url.c_str(), core::Json(protocols).dump().c_str());
}

BrowserWebSocket::~BrowserWebSocket() {
    getSinks().erase(id);
    haylen_js_socket_release(id);
}

void BrowserWebSocket::send(std::string data, bool binary) {
    haylen_js_socket_send(id, data.data(), static_cast<int>(data.size()), binary ? 1 : 0);
}

void BrowserWebSocket::ping(std::string) {
    throw std::logic_error("Browsers cannot send WebSocket ping frames.");
}

void BrowserWebSocket::close(int code, std::string reason) {
    haylen_js_socket_close(id, code, reason.c_str());
}

std::size_t BrowserWebSocket::getBufferedAmount() const noexcept {
    return static_cast<std::size_t>(haylen_js_socket_buffered(id));
}

void BrowserWebSocket::deliver(int socket, Event event) {
    if (const auto found = getSinks().find(socket); found != getSinks().end()) {
        found->second(std::move(event));
    }
}

} // namespace haylen::net

// Entry points for the browser socket events, called through `Module.haylen` in `platform/web/haylen-runtime.js`.
extern "C" {

EMSCRIPTEN_KEEPALIVE void haylen_web_socket_opened(int id, const char* protocol) {
    haylen::net::BrowserWebSocket::deliver(id, {.kind = haylen::net::WebSocketTransport::Event::Kind::Opened, .text = protocol});
}

EMSCRIPTEN_KEEPALIVE void haylen_web_socket_received(int id, const char* data, int size, int binary) {
    haylen::net::BrowserWebSocket::deliver(id, {.kind = haylen::net::WebSocketTransport::Event::Kind::Received, .text = std::string(data, static_cast<std::size_t>(size)), .binary = binary != 0});
}

EMSCRIPTEN_KEEPALIVE void haylen_web_socket_closed(int id, int code, const char* reason) {
    haylen::net::BrowserWebSocket::deliver(id, {.kind = haylen::net::WebSocketTransport::Event::Kind::Closed, .text = reason, .code = code});
}

EMSCRIPTEN_KEEPALIVE void haylen_web_socket_failed(int id, const char* message) {
    haylen::net::BrowserWebSocket::deliver(id, {.kind = haylen::net::WebSocketTransport::Event::Kind::Failed, .text = message});
}
}
