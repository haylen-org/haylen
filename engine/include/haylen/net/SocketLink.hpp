#pragma once

#include <memory>

#include "haylen/core/Connection.hpp"
#include "haylen/net/WebSocket.hpp"

namespace haylen::net {

// Ties a WebSocket to an owner, which closes the socket with the code 1000 when it ends.
class SocketLink final : public core::Connection::Link {
  public:
    explicit SocketLink(std::weak_ptr<WebSocket> value) : socket(std::move(value)) {}

    void disconnect() override;
    [[nodiscard]] bool isConnected() const noexcept override;
    void setBlocked(bool) override {}
    [[nodiscard]] bool isBlocked() const noexcept override {
        return false;
    }

  private:
    std::weak_ptr<WebSocket> socket;
};

} // namespace haylen::net
