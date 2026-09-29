#pragma once

#include <cstddef>
#include <vector>

#include "haylen/core/Connection.hpp"

namespace haylen::core {

// Ties connections to the lifetime of an owner, such as a scene, an autoload or any C++ object that keeps the scope as a member. Clearing the scope or destroying it disconnects everything added to it, so listeners, tweens and timers never outlive their owner.
class ConnectionScope final {
  public:
    ConnectionScope() = default;
    ~ConnectionScope();

    ConnectionScope(const ConnectionScope&) = delete;
    ConnectionScope& operator=(const ConnectionScope&) = delete;
    ConnectionScope(ConnectionScope&& other) noexcept;
    ConnectionScope& operator=(ConnectionScope&& other) noexcept;

    void add(Connection connection);

    // Disconnects everything, including connections that the disconnect callbacks add, and leaves the scope ready for new connections.
    void clear();

    [[nodiscard]] std::size_t size() const noexcept {
        return connections.size();
    }
    [[nodiscard]] bool empty() const noexcept {
        return connections.empty();
    }

  private:
    // Connections that ended on their own are dropped once the list reaches this size, so a long-lived owner that keeps adding short tweens stays small.
    static constexpr std::size_t kCompactThreshold = 64;

    std::vector<Connection> connections;
    std::size_t compactAt = kCompactThreshold;
};

} // namespace haylen::core
