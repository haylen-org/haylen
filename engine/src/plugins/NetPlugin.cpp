#include "haylen/plugins/NetPlugin.hpp"

#include <string>
#include <utility>

#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/JsonNumber.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "net/NetLua.hpp"

namespace haylen::plugins {

NetPlugin::NetPlugin(std::string networkRequirement) : requirement(std::move(networkRequirement)) {}

void NetPlugin::start(core::Engine& engine) {
    events = &engine.getEvents();
}

std::shared_ptr<net::WebSocket> NetPlugin::connectWebSocket(std::string url, net::WebSocket::Options options) {
    options.failureHint = requirement;
    auto socket = std::make_shared<net::WebSocket>(std::move(url), std::move(options));
    if (events != nullptr) {
        publishEvents(*socket);
    }
    sockets.push_back(socket);
    return socket;
}

// The socket drops these listeners when it closes, and the plugin drops them when it stops, so they never outlive the event bus.
void NetPlugin::publishEvents(net::WebSocket& socket) {
    core::EventBus* bus = events;
    const std::string url = socket.getUrl();
    socket.opened.connect([bus, url, &socket] { bus->emit(core::LifecycleEvent::kWebSocketConnected, {{"url", url}, {"protocol", socket.getProtocol()}}); });
    socket.disconnected.connect([bus, url](int code, std::string_view reason) { bus->emit(core::LifecycleEvent::kWebSocketDisconnected, {{"url", url}, {"code", code}, {"reason", std::string(reason)}}); });
    socket.reconnecting.connect([bus, url](int attempt, float delay) { bus->emit(core::LifecycleEvent::kWebSocketReconnecting, {{"url", url}, {"attempt", attempt}, {"delay", core::JsonNumber::fromFloat(delay)}}); });
}

void NetPlugin::stop(core::Engine&) {
    for (const std::shared_ptr<net::WebSocket>& socket : sockets) {
        socket->dropListeners();
        socket->close();
    }
    sockets.clear();
    events = nullptr;
}

// Sockets opened by a listener during the pump wait for the next frame.
void NetPlugin::beginFrame(core::Engine&, float) {
    const std::vector<std::shared_ptr<net::WebSocket>> snapshot = sockets;
    for (const std::shared_ptr<net::WebSocket>& socket : snapshot) {
        socket->pump();
    }
    std::erase_if(sockets, [](const std::shared_ptr<net::WebSocket>& socket) { return socket->getState() == net::WebSocket::State::Closed; });
}

void NetPlugin::installLua(core::Engine&, lua_State* L) {
    net::NetLua::install(L);
}

} // namespace haylen::plugins
