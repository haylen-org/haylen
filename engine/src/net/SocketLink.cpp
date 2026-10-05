#include "net/SocketLink.hpp"

namespace haylen::net {

void SocketLink::disconnect() {
    if (const std::shared_ptr<WebSocket> held = socket.lock()) {
        held->close();
    }
    socket.reset();
}

bool SocketLink::isConnected() const noexcept {
    const std::shared_ptr<WebSocket> held = socket.lock();
    return held && held->getState() != WebSocket::State::Closing && held->getState() != WebSocket::State::Closed;
}

} // namespace haylen::net
