#pragma once

#include <cstdint>
#include <string>

namespace haylen::port {

// What the platform tells of the network of the app: why it blocked a connection and what the project of the app lacks for network access. A port registers it where its platform blocks connections or gates network access in the project of the app, and the engine tells nothing for a platform without it.
class NetworkService {
  public:
    NetworkService() = default;
    NetworkService(const NetworkService&) = delete;
    NetworkService& operator=(const NetworkService&) = delete;
    virtual ~NetworkService() = default;

    // Explains, from any thread, why the platform blocked the connection of the socket with the descriptor, or answers an empty text.
    [[nodiscard]] virtual std::string explainBlockedSocket(std::intptr_t socket) = 0;

    // The sentence that the network errors of the engine end with while the project of the app lacks what network access needs, or an empty text.
    [[nodiscard]] virtual std::string getNetworkRequirement() = 0;
};

} // namespace haylen::port
