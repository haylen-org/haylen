#pragma once

#include <utility>

#include "haylen/core/Connection.hpp"

namespace haylen::core {

// Disconnects its connection when destroyed.
class ScopedConnection final {
  public:
    ScopedConnection() = default;
    ScopedConnection(Connection value) : connection(std::move(value)) {}
    ~ScopedConnection() {
        connection.disconnect();
    }

    ScopedConnection(const ScopedConnection&) = delete;
    ScopedConnection& operator=(const ScopedConnection&) = delete;
    ScopedConnection(ScopedConnection&& other) noexcept : connection(std::exchange(other.connection, {})) {}

    ScopedConnection& operator=(ScopedConnection&& other) noexcept {
        if (this != &other) {
            connection.disconnect();
            connection = std::exchange(other.connection, {});
        }
        return *this;
    }

    void disconnect() {
        connection.disconnect();
    }
    [[nodiscard]] bool isConnected() const noexcept {
        return connection.isConnected();
    }

  private:
    Connection connection;
};

} // namespace haylen::core
