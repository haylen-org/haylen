#include "net/WebSocketTransport.hpp"

#include <stdexcept>

namespace haylen::net {

// A port whose Varn builds no TLS reaches no WebSocket, which runs on the sockets and the TLS of Poco.
std::unique_ptr<WebSocketTransport> WebSocketTransport::open(const std::string&, const std::vector<std::string>&, std::size_t, float, BlockedReason, Sink) {
    throw std::runtime_error("This platform opens no WebSocket, since its port builds Varn without TLS. Name \"TLS=ON\" in the \"VARN_DRIVERS\" of its \"haylen_port\".");
}

} // namespace haylen::net
