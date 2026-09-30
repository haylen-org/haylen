#pragma once

#include <memory>
#include <string>
#include <vector>

#include "haylen/net/WebSocket.hpp"
#include "haylen/plugins/Plugin.hpp"

namespace haylen::core {
class EventBus;
}

namespace haylen::plugins {

// Opens WebSockets and delivers their events at the start of every frame. The plugin keeps each socket alive until it closes, so an app can hold on to its listeners alone, and it closes whatever is still open when the app stops. Every socket publishes `webSocketConnected`, `webSocketDisconnected` and `webSocketReconnecting` on the event bus with its address.
class NetPlugin final : public Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "net";
    }
    void start(core::Engine& engine) override;
    void stop(core::Engine& engine) override;
    void beginFrame(core::Engine& engine, float deltaSeconds) override;
    void installLua(core::Engine& engine, lua_State* L) override;

    [[nodiscard]] std::shared_ptr<net::WebSocket> connectWebSocket(std::string url, net::WebSocket::Options options = {});
    [[nodiscard]] std::size_t getOpenSocketCount() const noexcept {
        return sockets.size();
    }

  private:
    void publishEvents(net::WebSocket& socket);

    core::EventBus* events = nullptr;
    std::vector<std::shared_ptr<net::WebSocket>> sockets;
};

} // namespace haylen::plugins
