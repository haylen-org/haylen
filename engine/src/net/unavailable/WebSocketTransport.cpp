#include "net/WebSocketTransport.hpp"

#include <stdexcept>

namespace haylen::net {

// A port whose Varn builds no socket of Poco with TLS reaches no WebSocket, which needs both.
std::unique_ptr<WebSocketTransport> WebSocketTransport::open(const std::string&, const std::vector<std::string>&, std::size_t, float, BlockedReason, Sink) {
    throw std::runtime_error("This platform opens no WebSocket, since its port builds no socket with TLS.");
}

} // namespace haylen::net
