#include "platform/sokol/BridgeRelay.hpp"

#include "haylen/platform/Bridge.hpp"

namespace haylen::platform {

std::mutex BridgeRelay::mutex;
Bridge* BridgeRelay::bridge = nullptr;

void BridgeRelay::attach(Bridge* value) noexcept {
    const std::scoped_lock lock(mutex);
    bridge = value;
}

void BridgeRelay::resolve(std::uint64_t id, bool ok, std::string_view resultJson) {
    const std::scoped_lock lock(mutex);
    if (bridge != nullptr) {
        bridge->resolve(id, ok, resultJson);
    }
}

void BridgeRelay::emit(std::string_view event, std::string_view payloadJson) {
    const std::scoped_lock lock(mutex);
    if (bridge != nullptr) {
        bridge->emit(event, payloadJson);
    }
}

} // namespace haylen::platform
