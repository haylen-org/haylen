#include "haylen/core/ConnectionScope.hpp"

#include <algorithm>
#include <utility>

namespace haylen::core {

ConnectionScope::~ConnectionScope() {
    clear();
}

ConnectionScope::ConnectionScope(ConnectionScope&& other) noexcept : connections(std::exchange(other.connections, {})), compactAt(std::exchange(other.compactAt, kCompactThreshold)) {}

ConnectionScope& ConnectionScope::operator=(ConnectionScope&& other) noexcept {
    if (this != &other) {
        clear();
        connections = std::exchange(other.connections, {});
        compactAt = std::exchange(other.compactAt, kCompactThreshold);
    }
    return *this;
}

void ConnectionScope::add(Connection connection) {
    if (connections.size() >= compactAt) {
        std::erase_if(connections, [](const Connection& existing) { return !existing.isConnected(); });
        compactAt = std::max(kCompactThreshold, connections.size() * 2);
    }
    connections.push_back(std::move(connection));
}

void ConnectionScope::clear() {
    while (!connections.empty()) {
        std::vector<Connection> current = std::exchange(connections, {});
        for (Connection& connection : current) {
            connection.disconnect();
        }
    }
    compactAt = kCompactThreshold;
}

} // namespace haylen::core
